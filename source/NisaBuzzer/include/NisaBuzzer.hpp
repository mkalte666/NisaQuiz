//file: NisaBuzzer.h
//Author: Malte "mkalte" Kieﬂling
//note: Header Holding the class that communicates with the Buzzers of the NiSaCon Buzzer Sytem

#pragma once

//if you want to use another version of the asio lib or just use the one distributed with your system, define NISABUZZER_DIFFERENT_ASIO_LOCATION
//example: #define NISABUZZER_DIFFERENT_ASIO_LOCATION <asio/asio.hpp>
#define ASIO_STANDALONE
#ifdef NISABUZZER_DIFFERENT_ASIO_LOCATION
#include NISABUZZER_DIFFERENT_ASIO_LOCATION
#else
#include "./asio-1.10.6/include/asio.hpp"
#endif

//includes
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>

//namespace: NiseBuzzer
//note: Holds all classes, functions, ... for the NisaBuzzer lib
namespace NisaBuzzer
{
	//const var: BUZZER_BAUD_RATE
	//note: the baud rate of the buzzer system is fixed with the hardware
	const unsigned int BUZZER_BAUD_RATE = 38400;
	
	//class: BuzzerException
	//note: Exception thrown on critical errors with the Buzzer System
	class BuzzerException
	{
	public:
		BuzzerException() {}
		BuzzerException(const char* whatStringText) : whatString(whatStringText) { }
		~BuzzerException() throw() {};

		void SetWhat(const char* newWhatString) { whatString = newWhatString; }
		virtual const char* what(void) const throw() {
			return whatString;
		};

	private:
		const char* whatString;
	};
	
	//class: BuzzerEventHandler
	//note: Events caused by BuzzerManager can be handled with this class
	class BuzzerEventHandler
	{
	public:
		//typedef: EventHandlerFunction
		//note: Parameter 1 is the type of the event, parameter 2 the events parameter.
		typedef std::function<void(int,int)> EventHandlerFunction;
		
		//constructor: BuzzerEventHandler
		//note: Creates an empty BuzzerEventHandler. HandlerFunc still must be set with SetHandlerFunction
		BuzzerEventHandler()
			: handler()
		{
			id = maxId()++;
		}
		
		//constructor: BuzzerEventHandler
		//note: Creates an BuzzerEventHandler with a handlerFunc
		BuzzerEventHandler(EventHandlerFunction f)
			: handler(f)
		{
			id = maxId()++;
		}
		
		//copy constructor: BuzzerEventHandler
		//note: Copy Constructor 
		BuzzerEventHandler(const BuzzerEventHandler& h)
		{
			if(&h!=this) {
				this->handler=h.handler;
				this->id=h.id;
			}
		}
		
		//destructor: ~BuzzerEventHandler
		//note: does nothing cause nothing needs to be done
		~BuzzerEventHandler()
		{
		}
		
		//function: SetHandlerFunction
		//note: Sets the handlers function - called when the () operator is called by BuzzerManager
		//param: 	f: function to set
		void SetHandlerFunction(EventHandlerFunction f)
		{
			handler=f;
		}
		
		//operator: ()
		//note: calles the hander-func of this handler if availble
		void operator()(int type, int arg) const
		{
			if(handler) {
				handler(type,arg);
			}
		}
		//operatpr: ==
		//note: compares the handler-funcs (ids cause you cant compare std::function) 
		bool operator==(const BuzzerEventHandler&h) const
		{
			return (this->id==h.id);
		}
		bool operator!=(const BuzzerEventHandler&h) const { return !((*this)==h); }
		
	private:
		//var: handler. The Handlerfunc for this handler
		EventHandlerFunction handler;
		//var: id. Id of this handler. Used to assignt the handler-functions to handlers since std::function cannot be compared.
		unsigned int 		id;
		
		static int& maxId ()
		{ static int id= 0; return id; }
		
	};
	
	//class: BuzzerManager
	//note: Manages the communication with the buzzers and also fires events
	class BuzzerManager
	{
	public:
		//subclass: Event
		//note: class that holds the type and the param of an event. is basically a container
		class Event {
		public:
			//enum: Event
			//note: Events that can be caused
			enum EventType {
				EVENT_ARM = 0,	//param is 0
				EVENT_FULL_RESET,	//param is 0
				EVENT_PARTIAL_RESET, //param is 0
				EVENT_TRIGGER,  //param is id of the button triggerd
				EVENT_TILT,		//param is id of the button that should not have been pressed
				NUM_EVENTS		//not an event
			};
			
			int type;
			int param;
		};
		
		//constructor: BuzzerManager
		//note: Creates a BuzzzerManager, searching for the buzzer hardware at the givn port
		//param:	portname: the serial port with the hardware
		BuzzerManager(std::string portname)
			: isRunning(false), ioService(), port(ioService), enableThreadedEvents(false), hwVersion(0),swVersionMayor(0), swVersionMinor(0)
		{
			port.open(portname);
			port.set_option(asio::serial_port::baud_rate(BUZZER_BAUD_RATE));
			port.set_option(asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));
			port.set_option(asio::serial_port::parity(asio::serial_port::parity::none));
			port.set_option(asio::serial_port::character_size(asio::serial_port::character_size(8)));
			if(!port.is_open()) {
				throw BuzzerException("Cannot open port!");
			}
			isRunning = true;
			try {
				sendThread = std::thread(&BuzzerManager::SendThread, this);
				recvThread = std::thread(&BuzzerManager::RecvThread, this);
				eventThread = std::thread(&BuzzerManager::EventThread, this);
			}
			catch (...) {
				throw BuzzerException("Cannot start worker threads!");
			}
			InitBuzzers();
		}

		//destructor: BuzzerManager
		//note: destroys the buzzer manager
		~BuzzerManager()
		{
			PutByte('I');
			isRunning = false;
			
			sendThread.join();
			recvThread.join();
		}

		//function: InitBuzzers
		//note: Sends the Init signal to the buzzer. Can be called anytime for a full reset
		void InitBuzzers()
		{
			PutByte('I');
		}
		
		//function: SetEnableThreadedEvents
		//note: if set to true, there is no need to call Update anymore sine that is handled from the EventThread.
		//		usefull if the events can be called anytime. If, at the time this is enabled, there are events left in the eventqueue, they are fired!
		//param:	b: if true, enables the mutlithreded events
		void SetEnableThreadedEvents(bool b)
		{
			eventMutex.lock();
			enableThreadedEvents = b;
			if(enableThreadedEvents) {
				while(!eventQueue.empty()) {
					auto e = eventQueue.front();
					for(auto h : eventHandlers) {
						h(e.type, e.param);
					}
					eventQueue.pop();
				}
			}
			eventMutex.unlock();
		}
		
		//function: Update
		//note: Updates the Manager, calling event handlers. Not needed if SetEnableThreadedEvents is true
		void Update()
		{
			if(!enableThreadedEvents) {
				eventMutex.lock();
				while(!eventQueue.empty()) {
					auto e = eventQueue.front();
					for(auto h : eventHandlers) {
						h(e.type, e.param);
					}
					eventQueue.pop();
					}
				eventMutex.unlock();
			}
		}
		
		//function: Arm
		//note: "Arms"(=activates) the buzzers so they can fire a trigger event
		void Arm()
		{
			PutByte('A');
			FireEvent(Event::EventType::EVENT_ARM, 0);
		}
		
		//function: Reset
		//note: Resets the buzzers blocking the buzzer that was pressed recently
		void Reset()
		{
			PutByte('r');
			FireEvent(Event::EventType::EVENT_PARTIAL_RESET, 0);
		}
		
		//function: FullReset
		//note: resets all buzzers
		void FullReset()
		{
			PutByte('R');
			FireEvent(Event::EventType::EVENT_FULL_RESET, 0);
		}
		
		//function: AddEventHandler
		//note: 	adds a BuzzerEventHandler 
		//param:	h: the handler to add
		void AddEventHandler(BuzzerEventHandler h)
		{
			eventMutex.lock();
			//see if we already know this handler, if yes, just update it
			for(auto i = eventHandlers.begin();i<eventHandlers.end();i++) {
				if ((*i)==h) {
					(*i)=h;
					eventMutex.unlock();
					return;
				}
			}
			eventHandlers.push_back(h);
			eventMutex.unlock();
		}
		
		//function: RemoveEventHandler
		//note: removes a handler from the manager
		//param: h: the handler to remove
		void RemoveEventHandler(BuzzerEventHandler h)
		{
			for(auto i = eventHandlers.begin();i<eventHandlers.end();i++) {
				if ((*i)==h) {
					eventMutex.lock();
					eventHandlers.erase(i);
					eventMutex.unlock();
					return;
				}
			}
		}
		
		
	//this protected-block holds all functions to communicate with the serial port and the event-protocol-foo
	protected:
		//function: PutBytes
		//note: sends bytes to the ports send-queue
		//param:	bytes: std::vecor with the bytes to send
		void PutBytes(std::vector<unsigned char> bytes)
		{
			sendMutex.lock();
			for(auto b : bytes) {
				toSend.push(b);
			}
			sendMutex.unlock();
		}
		//function: PutByte
		//note: sends a single byte to the ports send-queue
		//param		byte: data to send
		void PutByte(unsigned char byte)
		{
			sendMutex.lock();
			toSend.push(byte);
			sendMutex.unlock();
		}
	
		//function: SendThread
		//note: Worker that puts data on the serial port
		void SendThread() 
		{
			while(isRunning) {
				if(!toSend.empty()) {
					sendMutex.lock();
					while(!toSend.empty()) {
						unsigned char c = toSend.front();
						port.write_some(asio::buffer(&c,1));
						toSend.pop();
					}
					sendMutex.unlock();
				}
			}
		}
		
		//function: RecvThread
		//note: Worker that reads data from the serial port
		void RecvThread()
		{
			while(isRunning) {
				unsigned char c;
				port.read_some(asio::buffer(&c,1));
				recvMutex.lock();
				toRead.push(c);
				recvMutex.unlock();
			}
		}
		
		//function: FireEvent
		//note: Fires an event
		//param:	event: Should be one of BuzzerManager::Event
		//			param: see BuzzerManager::Event what param is for each event 
		void FireEvent(int event, int param)
		{
			eventMutex.lock();
			if(enableThreadedEvents) {
				for(auto h : eventHandlers) {
					h(event,param);
				}
			}
			else {
				Event e;
				e.type = event;
				e.param = param;
				eventQueue.push(e);
			}
			eventMutex.unlock();
		}
		
		//function: EventThread
		//note: manages the events.
		void EventThread()
		{
			while(isRunning) {
				if(!toRead.empty()) {
					recvMutex.lock();
					unsigned char c = toRead.front();
					recvMutex.unlock();
					//is this the init-message?
					if(c=='H') {
						//we expect toRead to be size 6 or bigger, otherwise the init message isnt complete
						if(toRead.size()>=6) {
							recvMutex.lock();
							char hwChar, swMinorChar, swMayorChar;
							toRead.pop();
							hwChar = toRead.front();
							toRead.pop();toRead.pop();
							swMayorChar = toRead.front();
							toRead.pop();toRead.pop();
							swMinorChar = toRead.front();
							toRead.pop();
							recvMutex.unlock();
							hwVersion = hwChar - '0';
							swVersionMayor = swMayorChar - '0';
							swVersionMinor = swMinorChar - '0';
						}
					}
					//if c is a number, that means a buzzer is triggerd
					else if(c>='0' && c<= '9') {
						FireEvent(Event::EventType::EVENT_TRIGGER, c-'0');
						recvMutex.lock();
						toRead.pop();
						recvMutex.unlock();
					}
					//if its tilt, expect size to be >=2 since next byte is the id of the error-triggerd buzzer
					else if(c=='T') {
						if(toRead.size()>=2) {
							recvMutex.lock();
							toRead.pop();
							unsigned char param = toRead.front();
							toRead.pop();
							recvMutex.unlock();
							FireEvent(Event::EventType::EVENT_TILT,param);
						}
					} 
					//otherwise this byte is incorrect, just drop it
					else {
						recvMutex.lock();
						toRead.pop();
						recvMutex.unlock();
					}
				}
			}
		}

	private:
		//var: isRunning. true if the Manager is running, false if not
		bool isRunning;
		
		//var: io_service. needed for port
		asio::io_service 	ioService;
		//var: port. Port for the serial communication
		asio::serial_port 	port;
		
		//var: toSend. The send Queue. Every byte here will be put on the serial port
		std::queue<unsigned char> 	toSend;
		//var: toRead. The read Queue. Ever byte from the serial port ends up here
		std::queue<unsigned char>	toRead;
		
		//var: sendThread. the Thread that writes to  the serial port
		std::thread					sendThread;
		//var: sendMutex. the mutex loced so toSend dosn't explode
		std::recursive_mutex		sendMutex;
		//var: recvThread. the thread that reads from the serial port
		std::thread					recvThread;
		//var: recvMutex. the Mutex loced so toRead dosn't explode
		std::recursive_mutex		recvMutex;
		//var: eventThread. the thread that handlers the events
		std::thread					eventThread;
		//var: evntMutex. the mutex that maks shure eventHandlers dosnt explode
		std::recursive_mutex		eventMutex;
		
		//var: enableThreadedEvents. enables threaded events
		bool						enableThreadedEvents;
		//var: eventHandlers. EventHandlers
		std::vector<BuzzerEventHandler> 	eventHandlers;
		//var: eventQueue. the queue of the events to fire
		std::queue<Event>					eventQueue;
		
		//var: hwVersion. version of the hardware
		unsigned int hwVersion;
		//var: swVersionMayor. mayor version of the software on the device
		unsigned int swVersionMayor;
		//var: swVersionMinor. minor version of the software on the device
		unsigned int swVersionMinor;
	};


}; //namespace NisaBuzzer














