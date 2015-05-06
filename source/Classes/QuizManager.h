#pragma once

#include "BaseClass.h"
#include <NisaBuzzer\include\NisaBuzzer.hpp>
#include "Ui.h"

namespace Dragon2D
{

	//class: QuizQuestion
	//note: is a question
	class QuizQuestion
	{
	public:
		enum QuestionType {
			QUESTION_TEXT,
			QUESTION_MULTIPLE_CHOICE,
		};
		int type;
		std::string text;
		int points;

		std::string audioName;
		
		std::vector<std::string> answers;
		int rightAnswer;
	};


	//class: QuizManager
	//note: manages a running quiz
	D2DCLASS(QuizManager, public BaseClass)
	{
	public:
		QuizManager();
		~QuizManager();

		virtual void Load(std::string gamename, std::string player1, std::string player2, std::string player3, std::string player4);

		virtual void RegisterInputHooks() override;
		virtual void RemoveInputHooks() override;

		virtual void Update() override;

		int GetNumPlayers() const;
		std::string GetPlayername(int player) const;
		int GetPlayerpoints(int player) const;

		enum QuizState {
			STATE_SETUP = 0,
			STATE_POINT_DISPLAY,
			STATE_QUESTION,
			STATE_QUESTION_CHECKANSWER,
			STATE_QUESTION_SETUP,
			STATE_WRONG,
			STATE_RIGHT,
			STATE_QUESTION_CLEANUP,
		};

		enum QuizManageInput {
			IN_NONE = 0,
			IN_OK,
			IN_WRONG,
			IN_RESET,
			IN_START
		};


	private:
		static std::shared_ptr<NisaBuzzer::BuzzerManager> buzzerManager;

		int numPlayers;
		std::vector<std::string> names;
		std::vector<int> points;

		QuizState curstate;
		QuizManageInput lastInput;
		int lastBuzzerinput;
		int lastBuzzerinputParam;

		UiPtr curui;
		NisaBuzzer::BuzzerEventHandler buzzerEventHandler;

		int triggerdButton;

		std::queue<QuizQuestion> questions;
		int currentQuestion;

	protected:
		void SwitchUI();
	};

	D2DCLASS_SCRIPTINFO_BEGIN(QuizManager, BaseClass)
		D2DCLASS_SCRIPTINFO_MEMBER(QuizManager, Load)
		D2DCLASS_SCRIPTINFO_MEMBER(QuizManager, GetNumPlayers)
		D2DCLASS_SCRIPTINFO_MEMBER(QuizManager, GetPlayername)
		D2DCLASS_SCRIPTINFO_MEMBER(QuizManager, GetPlayerpoints)
	D2DCLASS_SCRIPTINFO_END

}; //namespace Dragon2D