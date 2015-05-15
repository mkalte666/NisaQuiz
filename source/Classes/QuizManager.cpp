#include "QuizManager.h"
#include "Ui.h"
#include "GameManager.h"
#include "Audio.h"

namespace Dragon2D
{
	D2DCLASS_REGISTER(QuizManager);

	std::shared_ptr<NisaBuzzer::BuzzerManager> QuizManager::buzzerManager = std::shared_ptr<NisaBuzzer::BuzzerManager>();

	QuizManager::QuizManager()
		: numPlayers(0), curstate(QuizState::STATE_SETUP), lastInput(QuizManageInput::IN_NONE), lastBuzzerinput(NisaBuzzer::BuzzerManager::Event::NUM_EVENTS), triggerdButton(-1), currentQuestion(-1)
	{

	}

	QuizManager::~QuizManager()
	{
		//Free the resources
	//	Env::GetResourceManager().FreeAudioResource("Buzz");
	}
	void QuizManager::Load(std::string gamename, std::string player1, std::string player2, std::string player3, std::string player4)
	{
		if (!buzzerManager) {
			std::string gameinit = Env::GetGamepath() + "GameInit.txt";
			std::string port = Env::Setting(gameinit)["comport"];
			buzzerManager.reset(new NisaBuzzer::BuzzerManager(port));
			buzzerManager->FullReset();
		}
		std::string gameinit = Env::GetGamepath() + "GameInit.txt";
		maxtries = atoi(Env::Setting(gameinit)["maxtries"].c_str());
		//Load the questions
		XMLResource& questionResource = Env::GetResourceManager().GetXMLResource(std::string("quiz/") + gamename + ".xml");
		auto doc = questionResource.GetDocument();
		for (auto t : doc.GetChildren()) {
			QuizQuestion newQuestion;
			if (t.GetName() == "question") {
				newQuestion.type = QuizQuestion::QUESTION_TEXT;
				newQuestion.text = t.GetData();
			}
			else if (t.GetName() == "multiplechoice") {
				newQuestion.type = QuizQuestion::QUESTION_MULTIPLE_CHOICE;
				newQuestion.text = t.GetData();
				for (auto answer : t.GetChildren()) {
					if (answer.GetName() == "answer") {
						newQuestion.answers.push_back(answer.GetData());
					}
					else if (answer.GetName() == "rightanswer") {
						newQuestion.rightAnswer = newQuestion.answers.size();
						newQuestion.answers.push_back(answer.GetData());
					}
				}
			}
			else {
				continue;
			}
			newQuestion.points = atoi(t.GetAttribute("points").c_str());
			newQuestion.audioName = t.GetAttribute("audio");
			if (newQuestion.audioName != "") {
				Env::GetResourceManager().RequestAudioResource(newQuestion.audioName);
			}
			questions.push(newQuestion);
		}
		//todo: randomize order


		//Ask for the music
		Env::GetResourceManager().RequestAudioResource("Buzz");

		numPlayers = 0;
		if (player1 != "") (numPlayers = 1);
		if (player2 != "") (numPlayers = 2);
		if (player3 != "") (numPlayers = 3);
		if (player4 != "") (numPlayers = 4);
		if (numPlayers == 0) {
			return;
		}
		names.resize(4);
		points.resize(4, 0);
		names[0] = player1;
		names[1] = player2;
		names[2] = player3;
		names[3] = player4;
		
		curstate = QuizState::STATE_POINT_DISPLAY;
		SwitchUI();
	}

	void QuizManager::Update()
	{
		buzzerManager->Update();
		switch (curstate)
		{
		case Dragon2D::QuizManager::STATE_SETUP:
			break;
		case Dragon2D::QuizManager::STATE_QUESTION_CLEANUP:
			buzzerManager->FullReset();
			curstate = STATE_POINT_DISPLAY;
			if (!questions.empty()) {
				questions.pop();
			}
			SwitchUI(); 
		case Dragon2D::QuizManager::STATE_POINT_DISPLAY:
			if (lastInput == QuizManageInput::IN_START) {	
				if(questions.size()>0) {
					triesLeft = maxtries;
					curstate = STATE_QUESTION;
					SwitchUI();
					buzzerManager->Arm();
					if (questions.front().audioName != "") {
						Music(questions.front().audioName).Play(100, -1);
					}
				} else {
					curstate = STATE_SHOW_WINNER;
					SwitchUI();
				}
			}
			break;
		case Dragon2D::QuizManager::STATE_QUESTION:
			if (questions.size() <= 0) {
				curstate = STATE_QUESTION_CLEANUP;
				break;
			}
			if (lastInput == IN_RESET) {

			}
			else if (lastBuzzerinput == NisaBuzzer::BuzzerManager::Event::EVENT_TRIGGER) {
				if (lastBuzzerinputParam <= numPlayers) {
					Music("Buzz").Play(0, 0);
					curstate = STATE_QUESTION_CHECKANSWER;
					SwitchUI();
				}
				else {
					buzzerManager->Arm();
				}
			}
			break;
		case Dragon2D::QuizManager::STATE_QUESTION_CHECKANSWER:			
			if (questions.front().type == QuizQuestion::QUESTION_MULTIPLE_CHOICE) {
				if (lastInput >= IN_1 && lastInput <= IN_4) {
					int id = lastInput - IN_1;
					if (id == questions.front().rightAnswer) {
						curstate = STATE_RIGHT;
						
					}
					else {
						curstate = STATE_WRONG;
						questions.front().answers[id] = "";
					}
				}
			}
			else {
				if (lastInput == IN_OK) {
					curstate = STATE_RIGHT;
					break;
				}
				else if (lastInput == IN_WRONG) {
					curstate = STATE_WRONG;
					break;
				}
			}
			break;
		case Dragon2D::QuizManager::STATE_WRONG:
			points[lastBuzzerinputParam-1] -= questions.front().points;
			triesLeft --;

			if (triesLeft > 0) {
				curstate = STATE_QUESTION;
				SwitchUI();
				buzzerManager->Reset();
				buzzerManager->Arm();
			}
			else {
				if (questions.front().type == QuizQuestion::QUESTION_MULTIPLE_CHOICE) {
					curstate = STATE_SHOW_ANSWER;
					SwitchUI();
				}
				else {
					curstate = STATE_QUESTION_CLEANUP;
				}
			}
			break;
		case Dragon2D::QuizManager::STATE_RIGHT:
			points[lastBuzzerinputParam-1] += questions.front().points;
			if (questions.front().type == QuizQuestion::QUESTION_MULTIPLE_CHOICE) {
				curstate = STATE_SHOW_ANSWER;
				SwitchUI();
			}
			else {
				curstate = STATE_QUESTION_CLEANUP;
			}
			break;
		case Dragon2D::QuizManager::STATE_SHOW_ANSWER:
			if (lastInput == IN_START) {
				curstate = STATE_QUESTION_CLEANUP;
			}
			break;
		case Dragon2D::QuizManager::STATE_SHOW_WINNER:
			if(lastInput == IN_START) {
				GameManager::CurrentManager().Remove(Ptr());
				GameManager::CurrentManager().Remove(curui);
				NewD2DObject<Ui>()->Load("playerselect");
			}

			break;
		default:
			break;
		}
		lastInput = QuizManageInput::IN_NONE;
		lastBuzzerinput = NisaBuzzer::BuzzerManager::Event::NUM_EVENTS;
	}

	int QuizManager::GetNumPlayers() const
	{
		return numPlayers;
	}

	int QuizManager::GetPlayerpoints(int player) const
	{
		if (player < numPlayers) {
			return points[player];
		}
		return 0;
	}

	std::string QuizManager::GetPlayername(int player) const
	{
		if (player < numPlayers) {
			return names[player];
		}
		return std::string();
	}
	
	void QuizManager::RegisterInputHooks()
	{
		InputEventFunction event1 = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_1; };
		InputEventFunction event2 = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_2; };
		InputEventFunction event3 = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_3; };
		InputEventFunction event4 = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_4; };
		InputEventFunction okevent = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_OK; };
		InputEventFunction wrongevent = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_WRONG; };
		InputEventFunction resetevent = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_RESET; };
		InputEventFunction startevent = [this](bool pressed) { if (pressed) this->lastInput = QuizManageInput::IN_START; };
		buzzerEventHandler.SetHandlerFunction([this](int type, int param) {
			this->lastBuzzerinput = type;
			this->lastBuzzerinputParam = param;
		});
		Env::GetInput().AddHook("ok", Ptr(), okevent);
		Env::GetInput().AddHook("wrong", Ptr(), wrongevent);
		Env::GetInput().AddHook("reset", Ptr(), resetevent);
		Env::GetInput().AddHook("start", Ptr(), startevent);
		Env::GetInput().AddHook("1", Ptr(), event1);
		Env::GetInput().AddHook("2", Ptr(), event2);
		Env::GetInput().AddHook("3", Ptr(), event3);
		Env::GetInput().AddHook("4", Ptr(), event4);
		buzzerManager->AddEventHandler(buzzerEventHandler);
	}

	void QuizManager::RemoveInputHooks()
	{
		Env::GetInput().RemoveHooks(Ptr());
		buzzerManager->RemoveEventHandler(buzzerEventHandler);
	}

	void QuizManager::SwitchUI()
	{
		if (!curui) {
			curui = NewD2DObject<Ui>();
			curui->Load("quizui");
		}
		//hide all and load the right one for curstate IF all do exist
		auto pointscreen = curui->GetLoader().GetElementById("pointscreen");
		auto questionbase = curui->GetLoader().GetElementById("questionbase");
		auto checkquestionbase = curui->GetLoader().GetElementById("checkquestionbase");
		if (pointscreen&&questionbase&&checkquestionbase) {
			pointscreen->SetHidden(false);
			questionbase->SetHidden(true);
			checkquestionbase->SetHidden(true);

			pointscreen->GetElementById("p0n")->SetName(names[0]);
			pointscreen->GetElementById("p1n")->SetName(names[1]);
			pointscreen->GetElementById("p2n")->SetName(names[2]);
			pointscreen->GetElementById("p3n")->SetName(names[3]);
			pointscreen->GetElementById("p0p")->SetName(std::to_string(points[0]));
			numPlayers >= 2 ? pointscreen->GetElementById("p1p")->SetName(std::to_string(points[1])) : pointscreen->GetElementById("p1p")->SetName("");
			numPlayers >= 3 ? pointscreen->GetElementById("p2p")->SetName(std::to_string(points[2])) : pointscreen->GetElementById("p2p")->SetName("");
			numPlayers >= 4 ? pointscreen->GetElementById("p3p")->SetName(std::to_string(points[3])) : pointscreen->GetElementById("p3p")->SetName("");


			switch (curstate)
			{
			case STATE_POINT_DISPLAY:
				
				break;
			case STATE_QUESTION_CHECKANSWER:
				checkquestionbase->GetElementById("buzzerPlayer")->SetName(names[lastBuzzerinputParam - 1]);
				checkquestionbase->SetHidden(false);
			case STATE_QUESTION:
				questionbase->GetElementById("questionText")->SetName(questions.front().text);
				if (questions.front().type == QuizQuestion::QUESTION_MULTIPLE_CHOICE) {
					questionbase->GetElementById("choiceContainer")->SetHidden(false);
					questions.front().answers.size() >= 1 ? questionbase->GetElementById("answer0")->SetName(questions.front().answers[0]) : questionbase->GetElementById("answer0")->SetName("");
					questions.front().answers.size() >= 2 ? questionbase->GetElementById("answer1")->SetName(questions.front().answers[1]) : questionbase->GetElementById("answer1")->SetName("");
					questions.front().answers.size() >= 3 ? questionbase->GetElementById("answer2")->SetName(questions.front().answers[2]) : questionbase->GetElementById("answer2")->SetName("");
					questions.front().answers.size() >= 4 ? questionbase->GetElementById("answer3")->SetName(questions.front().answers[3]) : questionbase->GetElementById("answer3")->SetName("");
				}
				else {
					questionbase->GetElementById("choiceContainer")->SetHidden(true);
				}
				questionbase->SetHidden(false);
				break;
		
			case STATE_SHOW_ANSWER:
				questionbase->GetElementById("questionText")->SetName(questions.front().text);
				if (questions.front().type == QuizQuestion::QUESTION_MULTIPLE_CHOICE) {
					questionbase->GetElementById("choiceContainer")->SetHidden(false);
					questions.front().rightAnswer == 0 ? questionbase->GetElementById("answer0")->SetName(questions.front().answers[0]) : questionbase->GetElementById("answer0")->SetName("");
					questions.front().rightAnswer == 1 ? questionbase->GetElementById("answer1")->SetName(questions.front().answers[1]) : questionbase->GetElementById("answer1")->SetName("");
					questions.front().rightAnswer == 2 ? questionbase->GetElementById("answer2")->SetName(questions.front().answers[2]) : questionbase->GetElementById("answer2")->SetName("");
					questions.front().rightAnswer == 3 ? questionbase->GetElementById("answer3")->SetName(questions.front().answers[3]) : questionbase->GetElementById("answer3")->SetName("");
				}
				else {
					questionbase->GetElementById("choiceContainer")->SetHidden(true);
				}
				questionbase->SetHidden(false);
				break;
			default:
				break;
			}
		}
	}
};//namespace Dragon2D
