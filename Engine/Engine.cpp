// ------------------------------------ //
#ifndef LEVIATHAN_ENGINE
#include "Engine.h"
#endif
#include "Application/AppDefine.h"
#include "Application/Application.h"
#include "Common/DataStoring/DataStore.h"
#include "Common/Misc.h"
#include "Common/StringOperations.h"
#include "Entities/GameWorld.h"
#include "Entities/Serializers/SendableEntitySerializer.h"
#include "Events/EventHandler.h"
#include "Handlers/EntitySerializerManager.h"
#include "Handlers/ObjectLoader.h"
#include "Handlers/OutOfMemoryHandler.h"
#include "Handlers/ResourceRefreshHandler.h"
#include "Handlers/ConstraintSerializerManager.h"
#include "Leap/LeapManager.h"
#include "Networking/NetworkHandler.h"
#include "Networking/RemoteConsole.h"
#include "Newton/NewtonManager.h"
#include "Newton/PhysicsMaterialManager.h"
#include "ObjectFiles/ObjectFileProcessor.h"
#include "Rendering/GraphicalInputEntity.h"
#include "Rendering/Graphics.h"
#include "Script/Console.h"
#include "Sound/SoundDevice.h"
#include "Statistics/RenderingStatistics.h"
#include "Threading/ThreadingManager.h"
#include "Utility/Random.h"
#include <boost/chrono/duration.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/locks.hpp>

#ifdef LEVIATHAN_USES_VLD
#include <vld.h>
#endif // LEVIATHAN_USES_VLD

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

using namespace Leviathan;
// ------------------------------------ //

#ifdef _WIN32

static const WORD MAX_CONSOLE_LINES = 500;

#endif

DLLEXPORT Leviathan::Engine::Engine(LeviathanApplication* owner) :
    Owner(owner), LeapData(NULL), MainConsole(NULL), MainFileHandler(NULL), _NewtonManager(NULL),
    GraphicalEntity1(NULL), PhysMaterials(NULL), _NetworkHandler(NULL), _ThreadingManager(NULL), NoGui(false),
    _RemoteConsole(NULL), PreReleaseWaiting(false), PreReleaseDone(false), NoLeap(false),
    _ResourceRefreshHandler(NULL), PreReleaseCompleted(false), _EntitySerializerManager(NULL),
    _ConstraintSerializerManager(NULL)
{
	IDDefaultInstance = IDFactory::Get();

	Inited = false;
	Graph = NULL;
	Define = NULL;
	MainRandom = NULL;
	RenderTimer = NULL;

	Sound = NULL;

	TimePassed = 0;
	LastFrame = 0;

	Mainstore = NULL;
	MainScript = NULL;

	TickCount = 0;
	TickTime = 0;
	FrameCount = 0;

	MainEvents = NULL;
	Loader = NULL;
	OutOMemory = NULL;
}

DLLEXPORT Leviathan::Engine::~Engine(){
	// Reset the instance ptr //
	instance = NULL;
}

Engine* Leviathan::Engine::instance = NULL;

Engine* Leviathan::Engine::GetEngine(){
	return instance;
}

DLLEXPORT Engine* Leviathan::Engine::Get(){
	return instance;
}
// ------------------------------------ //
DLLEXPORT bool Leviathan::Engine::Init(AppDef*  definition, NETWORKED_TYPE ntype){
	GUARD_LOCK_THIS_OBJECT();
    
	// Get the  time, for monitoring how long loading takes //
	__int64 InitStartTime = Misc::GetTimeMs64();

	// Set static access to this object //
	instance = this;
    
	// Store parameters //
	Define = definition;

	// Create all the things //
    
	OutOMemory = new OutOfMemoryHandler();

	// Create threading facilities //
	_ThreadingManager = new ThreadingManager();
	if(!_ThreadingManager->Init()){

		Logger::Get()->Error(L"Engine: Init: cannot start threading");
		return false;
	}

	// Create the randomizer //
	MainRandom = new Random((int)InitStartTime);
	MainRandom->SetAsMain();

	if(NoGui){
		// Console might be the first thing we want //
#ifdef _WIN32
		WinAllocateConsole();
#else
        // On linux we probably shouldn't spawn a new terminal and instead just exit //
        
        
#endif

		// Tell window title //
		Logger::Get()->Write(L"// ----------- "+Define->GetWindowDetails().Title+L" ----------- //");
	}
	

	// We could immediately receive a remote console request so this should be ready when networking is started //
	_RemoteConsole = new RemoteConsole();

	// We want to send a request to the master server as soon as possible //
	_NetworkHandler = new NetworkHandler(ntype, Define->GetPacketHandler());

	_NetworkHandler->Init(Define->GetMasterServerInfo());

	// These should be fine to be threaded //

	// File change listener //
	_ResourceRefreshHandler = new ResourceRefreshHandler();
	if(!_ResourceRefreshHandler->Init()){

		Logger::Get()->Error(L"Engine: Init: cannot start resource monitor");
		return false;
	}

	// Data storage //
	Mainstore = new DataStore(true);
	if(!Mainstore){

		Logger::Get()->Error(L"Engine: Init: failed to create main data store");
		return false;
	}

	// Search data folder for files //
	MainFileHandler = new FileSystem();
	if(!MainFileHandler){

		Logger::Get()->Error(L"Engine: Init: failed to create FileSystem");
		return false;
	}

	if(!MainFileHandler->Init()){

		Logger::Get()->Error(L"Engine: Init: failed to init FileSystem");
		return false;
	}

	// File parsing //
	ObjectFileProcessor::Initialize();

	// Main program wide event dispatcher //
	MainEvents = new EventHandler();
	if(!MainEvents){

		Logger::Get()->Error(L"Engine: Init: failed to create MainEvents");
		return false;
	}

	if(!MainEvents->Init()){

		Logger::Get()->Error(L"Engine: Init: failed to init MainEvents");
		return false;
	}

	// Check is threading properly started //
	if(!_ThreadingManager->CheckInit()){

		Logger::Get()->Error(L"Engine: Init: threading start failed");
		return false;
	}

	// create script interface before renderer //
	boost::promise<bool> ScriptInterfaceResult;
    
	// Ref is OK to use since this task finishes before this function //
    _ThreadingManager->QueueTask(shared_ptr<QueuedTask>(new QueuedTask(boost::bind<void>([](
                        boost::promise<bool> &returnvalue, Engine* engine) -> void
        {

            engine->MainScript = new ScriptInterface();
            if(!engine->MainScript){

                Logger::Get()->Error(L"Engine: Init: failed to create ScriptInterface");
                returnvalue.set_value(false);
                return;
            }

            if(!engine->MainScript->Init()){

                Logger::Get()->Error(L"Engine: Init: failed to init ScriptInterface");
                returnvalue.set_value(false);
                return;
            }

            // create console after script engine //
            engine->MainConsole = new ScriptConsole();
            if(!engine->MainConsole){

                Logger::Get()->Error(L"Engine: Init: failed to create ScriptConsole");
                returnvalue.set_value(false);
                return;
            }

            if(!engine->MainConsole->Init(engine->MainScript)){

                Logger::Get()->Error(L"Engine: Init: failed to initialize Console, continuing anyway");
            }

            returnvalue.set_value(true);
        }, boost::ref(ScriptInterfaceResult), this))));

	// create newton manager before any newton resources are needed //
	boost::promise<bool> NewtonManagerResult;
    
	// Ref is OK to use since this task finishes before this function //
	_ThreadingManager->QueueTask(new QueuedTask(boost::bind<void>([](
                    boost::promise<bool> &returnvalue, Engine* engine) -> void
        {

            engine->_NewtonManager = new NewtonManager();
            if(!engine->_NewtonManager){

                Logger::Get()->Error(L"Engine: Init: failed to create NewtonManager");
                returnvalue.set_value(false);
                return;
            }

            // next force application to load physical surface materials //
            engine->PhysMaterials = new PhysicsMaterialManager(engine->_NewtonManager);
            if(!engine->PhysMaterials){

                Logger::Get()->Error(L"Engine: Init: failed to create PhysicsMaterialManager");
                returnvalue.set_value(false);
                return;
            }

            engine->Owner->RegisterApplicationPhysicalMaterials(engine->PhysMaterials);

            returnvalue.set_value(true);
        }, boost::ref(NewtonManagerResult), this)));

    boost::promise<bool> SerializerManagerResult;

    _ThreadingManager->QueueTask(new QueuedTask(boost::bind<void>([](boost::promise<bool> &returnvalue,
                    Engine* engine) -> void
        {

            engine->_EntitySerializerManager = new EntitySerializerManager();
            if(!engine->_EntitySerializerManager){

                returnvalue.set_value(false);
                return;
            }

            // Create the default serializer //
            unique_ptr<BaseEntitySerializer> tmpptr(new SendableEntitySerializer());
            if(!tmpptr){

                returnvalue.set_value(false);
                return;
            }

            engine->_EntitySerializerManager->AddSerializer(tmpptr.release());

            engine->_ConstraintSerializerManager = new ConstraintSerializerManager();
            if(!engine->_ConstraintSerializerManager->Init()){

                returnvalue.set_value(false);
                return;
            }
            
            returnvalue.set_value(true);

        }, boost::ref(SerializerManagerResult), this)));

    
	// Check if we don't want a window //
	if(NoGui){

		Logger::Get()->Info(L"Engine: Init: starting in console mode (won't allocate graphical objects) ");
	} else {

		ObjectFileProcessor::LoadValueFromNamedVars<int>(Define->GetValues(), L"MaxFPS", FrameLimit, 120, true,
            L"Graphics: Init:");

		Graph = new Graphics();

	}
	// We need to wait for all current tasks to finish //
	_ThreadingManager->WaitForAllTasksToFinish();

	// Check return values //
	if(!ScriptInterfaceResult.get_future().get() || !NewtonManagerResult.get_future().get() ||
        !SerializerManagerResult.get_future().get())
	{

		Logger::Get()->Error(L"Engine: Init: one or more queued tasks failed");
		return false;
	}

	// We can queue some more tasks //
	// create leap controller //
	boost::thread leapinitthread;
	if(!NoLeap)
		leapinitthread = boost::thread(boost::bind<void>([](Engine* engine) -> void{

			engine->LeapData = new LeapManager(engine);
			if(!engine->LeapData){
				Logger::Get()->Error(L"Engine: Init: failed to create LeapManager");
				return;
			}
			// try here just in case //
			try{
				if(!engine->LeapData->Init()){

					Logger::Get()->Info(L"Engine: Init: No Leap controller found, not using one");
				}
			}
			catch(...){
				// threw something //
				Logger::Get()->Error(L"Engine: Init: Leap threw something, even without leap this shouldn't happen; "
                    L"continuing anyway");
			}

		}, this));


	// sound device //
	boost::promise<bool> SoundDeviceResult;
	// Ref is OK to use since this task finishes before this function //
	_ThreadingManager->QueueTask(shared_ptr<QueuedTask>(new QueuedTask(boost::bind<void>([](
                        boost::promise<bool> &returnvalue, Engine* engine) -> void{
                        if(!engine->NoGui){
                            engine->Sound = new SoundDevice();
                            if(!engine->Sound){
                                Logger::Get()->Error(L"Engine: Init: failed to create SoundDevice");
                                returnvalue.set_value(false);
                                return;
                            }

                            if(!engine->Sound->Init()){

                                Logger::Get()->Error(L"Engine: Init: failed to init SoundDevice");
                                returnvalue.set_value(false);
                                return;
                            }
                        }

                        // make angel script make list of registered stuff //
                        engine->MainScript->GetExecutor()->ScanAngelScriptTypes();

                        if(!engine->NoGui){
                            // measuring //
                            engine->RenderTimer = new RenderingStatistics();
                            if(!engine->RenderTimer){
                                Logger::Get()->Error(L"Engine: Init: failed to create RenderingStatistics");
                                returnvalue.set_value(false);
                                return;
                            }
                        }
                        // create object loader //
                        engine->Loader = new ObjectLoader(engine);
                        if(!engine->Loader){
                            Logger::Get()->Error(L"Engine: Init: failed to create ObjectLoader");
                            returnvalue.set_value(false);
                            return;
                        }

                        if(engine->NoGui){
                            // Set object loader to gui mode //
                            // \todo do this

                        }

                        returnvalue.set_value(true);
                    }, boost::ref(SoundDeviceResult), this))));

	if(!NoGui){
		if(!Graph){

			Logger::Get()->Error(L"Engine: Init: failed to create instance of Graphics");
			return false;
		}

		// call init //
		if(!Graph->Init(definition)){
			Logger::Get()->Error(L"Failed to init Engine, Init graphics failed! Aborting");
			return false;
		}

		// create window //
		GraphicalEntity1 = new GraphicalInputEntity(Graph, definition);
	}

	if(!SoundDeviceResult.get_future().get()){

		Logger::Get()->Error(L"Engine: Init: sound device queued tasks failed");
		return false;
	}

	Inited = true;

	// We can probably assume here that leap creation has stalled if the thread is running //
	if(!NoLeap && !leapinitthread.try_join_for(boost::chrono::milliseconds(5))){
		// We can assume that it is running //
		Logger::Get()->Warning(L"LeapController creation would have stalled the game!");
		//Misc::KillThread(leapinitthread);
	}

	PostLoad();

	Logger::Get()->Info(L"Engine init took "+Convert::ToWstring(Misc::GetTimeMs64()-InitStartTime)+L" ms", false);
	return true;
}

void Leviathan::Engine::PostLoad(){
	// increase start count //
	int startcounts = 0;

	if(Mainstore->GetValueAndConvertTo<int>(L"StartCount", startcounts)){
		// increase //
		Mainstore->SetValue(L"StartCount", new VariableBlock(new IntBlock(startcounts+1)));
	} else {

		Mainstore->AddVar(new NamedVariableList(L"StartCount", new VariableBlock(1)));
		// set as persistent //
		Mainstore->SetPersistance(L"StartCount", true);
	}

	// Start receiving input //
    CinThread = boost::thread(boost::bind<void>([]() -> void
        {
            // First get input //
            wstring inputcommand;

            while(true){

                getline(wcin, inputcommand);
                // Pass to various things //
                Engine* engine = Engine::Get();
				
                // Stop if Engine is no more //
                if(!engine){

                    break;
                }

                GUARD_LOCK_OTHER_OBJECT(engine);
                auto tmpptr = engine->MainConsole;
                if(tmpptr){

                    tmpptr->RunConsoleCommand(inputcommand);

                } else {
                    Logger::Get()->Warning("No console handler attached, cannot run command");
                }
            }

        }));


	// get time //
	LastFrame = Misc::GetTimeMs64();
}

void Leviathan::Engine::Release(bool forced){
	GUARD_LOCK_THIS_OBJECT();

	if(!forced)
		assert(PreReleaseDone && "PreReleaseDone must be done before actual release!");

	// Destroy worlds //
    {
        GUARD_LOCK_BASIC(GameWorldsLock);
        
        while(GameWorlds.size()){

            GameWorlds[0]->Release();
            GameWorlds.erase(GameWorlds.begin());
        }

    }
	
	// Wait for tasks to finish //
	if(!forced)
		_ThreadingManager->WaitForAllTasksToFinish();

	if(GraphicalEntity1){
		// Make windows clear their stored objects //
		GraphicalEntity1->ReleaseLinked();
	}

	// Destroy windows //
	SAFE_DELETE(GraphicalEntity1);

	// Release newton //
	SAFE_DELETE(PhysMaterials);
	SAFE_DELETE(_NewtonManager);

	SAFE_RELEASEDEL(LeapData);

	// Console needs to be released before script release //
	SAFE_RELEASEDEL(MainConsole);

	SAFE_RELEASEDEL(MainScript);

	// Save at this point (just in case it crashes before exiting) //
	Logger::Get()->Save();

	SAFE_DELETE(Loader);
	SAFE_RELEASEDEL(Graph);
	SAFE_DELETE(RenderTimer);

    SAFE_RELEASEDEL(_EntitySerializerManager);
    SAFE_RELEASEDEL(_ConstraintSerializerManager);

	SAFE_RELEASEDEL(Sound);
	SAFE_DELETE(Mainstore);

	SAFE_RELEASEDEL(MainEvents);
	// delete randomizer last, for obvious reasons //
	SAFE_DELETE(MainRandom);

	Gui::GuiManager::KillGlobalCache();

	ObjectFileProcessor::Release();
	SAFE_RELEASEDEL(MainFileHandler);

	// Stop threads //
	if(!forced)
		_ThreadingManager->WaitForAllTasksToFinish();
	SAFE_RELEASEDEL(_ThreadingManager);

	// clears all running timers that might have accidentally been left running //
	TimingMonitor::ClearTimers();

	// safe to delete this here //
	SAFE_DELETE(OutOMemory);

	SAFE_DELETE(IDDefaultInstance);

	Logger::Get()->Write(L"Goodbye cruel world!");
}
// ------------------------------------ //
void Leviathan::Engine::Tick(){
	GUARD_LOCK_THIS_OBJECT();

	// Always try to update networking //
	if(_NetworkHandler)
		_NetworkHandler->UpdateAllConnections();

	if(PreReleaseWaiting){

		PreReleaseWaiting = false;
		PreReleaseDone = true;

		Logger::Get()->Info(L"Engine: performing final release tick");

        
        
		// Call last tick event //
        
	}

	// Get the passed time since the last update //
	__int64 CurTime = Misc::GetTimeMs64();
	TimePassed = (int)(CurTime-LastFrame);


	if((TimePassed < TICKSPEED)){
		// It's not tick time yet //
		return;
	}


	LastFrame += TICKSPEED;
	TickCount++;

	// Update input //
	if(LeapData)
		LeapData->OnTick(TimePassed);

	if(!NoGui){
		// sound tick //
		Sound->Tick(TimePassed);

		// update windows //
		GraphicalEntity1->Tick(TimePassed);

	}


    // Update worlds //
    {
        GUARD_LOCK_BASIC(GameWorldsLock);

        // This will also update physics //
        auto end = GameWorlds.end();
        for(auto iter = GameWorlds.begin(); iter != end; ++iter){

            (*iter)->Tick();
        }
    }
    
    
	// Some dark magic here //
	if(TickCount % 25 == 0){
		// update values
		Mainstore->SetTickCount(TickCount);
		Mainstore->SetTickTime(TickTime);

		if(!NoGui){
			// send updated rendering statistics //
			RenderTimer->ReportStats(Mainstore);
		}
	}

	// Update file listeners //
	if(_ResourceRefreshHandler)
		_ResourceRefreshHandler->CheckFileStatus();

	// Send the tick event //
	MainEvents->CallEvent(new Event(EVENT_TYPE_TICK, new IntegerEventData(TickCount)));

	// Call the default app tick //
	Owner->Tick(TimePassed);

	TickTime = (int)(Misc::GetTimeMs64()-LastFrame);
}

DLLEXPORT void Leviathan::Engine::PreFirstTick(){
    
    GUARD_LOCK_THIS_OBJECT();

    // Stop this handling as it is no longer required //
    if(_NetworkHandler)
        _NetworkHandler->StopOwnUpdaterThread();

    if(_ThreadingManager)
        _ThreadingManager->NotifyQueuerThread();
    
	Logger::Get()->Info(L"Engine: PreFirstTick: everything fine to start running");
}
// ------------------------------------ //
void Leviathan::Engine::RenderFrame(){
	// We want to totally ignore this if we are in text mode //
	if(NoGui)
		return;

	int SinceLastFrame = -1;
	UNIQUE_LOCK_THIS_OBJECT();

	// limit check //
	if(!RenderTimer->CanRenderNow(FrameLimit, SinceLastFrame)){
		// fps would go too high //

		return;
	}

	// since last frame is in microseconds 10^-6 convert to milliseconds //
	// SinceLastFrame is always more than 1000 (always 1 ms or more) //
	SinceLastFrame /= 1000;
	FrameCount++;

	// advanced statistic start monitoring //
	RenderTimer->RenderingStart();

	MainEvents->CallEvent(new Event(EVENT_TYPE_FRAME_BEGIN, new IntegerEventData(SinceLastFrame)));


	bool shouldrender = false;

	// Render //
	if(GraphicalEntity1->Render(SinceLastFrame))
		shouldrender = true;

    lockit.unlock();
	if(shouldrender)
		Graph->Frame();

    lockit.lock();
	MainEvents->CallEvent(new Event(EVENT_TYPE_FRAME_END, new IntegerEventData(FrameCount)));

	// advanced statistics frame has ended //
	RenderTimer->RenderingEnd();
}
// ------------------------------------ //
DLLEXPORT void Leviathan::Engine::PreRelease(){
	GUARD_LOCK_THIS_OBJECT();
	if(PreReleaseWaiting || PreReleaseCompleted)
		return;
	
	PreReleaseWaiting = true;
	// This will stay true until the end of times //
	PreReleaseCompleted = true;

	// Stop command handling first //
    // Apparently killing doesn't really work on linux in any reasonable way, so just let it run
#if 0
    //Misc::KillThread(CinThread);
    Logger::Get()->Info(L"Successfully stopped command handling");
#endif


	// Then kill the network //
	_NetworkHandler->GetInterface()->CloseDown();


	// Let the game release it's resources //
	Owner->EnginePreShutdown();

	// Close remote console //
	SAFE_DELETE(_RemoteConsole);

	// Close all connections //
	SAFE_RELEASEDEL(_NetworkHandler);

	SAFE_RELEASEDEL(_ResourceRefreshHandler);

	// Set worlds to empty //
    {
        GUARD_LOCK_BASIC(GameWorldsLock);
        
        for(auto iter = GameWorlds.begin(); iter != GameWorlds.end(); ++iter){
            // Set all objects to release //
            (*iter)->MarkForClear();
        }
    }

	// Set tasks to a proper state //
	_ThreadingManager->SetDiscardConditionalTasks(true);
	_ThreadingManager->SetDisallowRepeatingTasks(true);

	Logger::Get()->Info(L"Engine: prerelease done, waiting for a tick");
}

DLLEXPORT bool Leviathan::Engine::HasPreReleaseBeenDone() const{
	return PreReleaseDone;
}
// ------------------------------------ //
DLLEXPORT void Leviathan::Engine::SaveScreenShot(){
	assert(!NoGui && "really shouldn't try to screenshot in text-only mode");
	GUARD_LOCK_THIS_OBJECT();

	const wstring fileprefix = MainFileHandler->GetDataFolder()+L"Screenshots/Captured_frame_";


	GraphicalEntity1->SaveScreenShot(Convert::WstringToString(fileprefix));
}

DLLEXPORT int Leviathan::Engine::GetWindowOpenCount(){
	int openwindows = 0;

	// If we are in text only mode always return 1 //
	if(NoGui)
		return 1;
	GUARD_LOCK_THIS_OBJECT();

	if(GraphicalEntity1->GetWindow()->IsOpen())
		openwindows++;

	return openwindows;
}
// ------------------------------------ //
DLLEXPORT shared_ptr<GameWorld> Leviathan::Engine::CreateWorld(GraphicalInputEntity* owningwindow,
    shared_ptr<ViewerCameraPos> worldscamera)
{
    
	shared_ptr<GameWorld> tmp(new GameWorld());
	tmp->Init(owningwindow, NoGui ? NULL: Graph->GetOgreRoot());
	if(owningwindow)
		owningwindow->LinkObjects(worldscamera, tmp);
    
    GUARD_LOCK_BASIC(GameWorldsLock);
    
	GameWorlds.push_back(tmp);
	return GameWorlds.back();
}

DLLEXPORT void Leviathan::Engine::DestroyWorld(shared_ptr<GameWorld> &world){

    if(!world)
        return;
    
    // Release the world first //
    world->Release();

    // Then delete it //
    GUARD_LOCK_BASIC(GameWorldsLock);
    
    auto end = GameWorlds.end();
    for(auto iter = GameWorlds.begin(); iter != end; ++iter){

        if((*iter).get() == world.get()){

            GameWorlds.erase(iter);
            world.reset();
            return;
        }
    }

    // Should be fine destroying worlds that aren't on the list... //
    world.reset();
}
// ------------------------------------ //
void Leviathan::Engine::_NotifyThreadsRegisterOgre(){
	if(NoGui)
		return;
    
	// Register threads to use graphical objects //
	_ThreadingManager->MakeThreadsWorkWithOgre();
}
// ------------------------------------ //
DLLEXPORT int Leviathan::Engine::GetTimeSinceLastTick() const{

    return Misc::GetTimeMs64()-LastFrame;
}

void Leviathan::Engine::_AdjustTickClock(int amount, bool absolute /*= true*/){

    GUARD_LOCK_THIS_OBJECT();
    
    if(!absolute){

        Logger::Get()->Info("Engine: adjusted tick timer by "+Convert::ToString(amount));
        
        LastFrame += amount;
        return;
    }

    // Calculate the time in the current last tick //
    int64_t templasttick = LastFrame;

    int64_t curtime = Misc::GetTimeMs64();

    while(curtime-templasttick >= TICKSPEED){

        templasttick += TICKSPEED;
    }

    // Check how far off we are from the target //
    int intolasttick = curtime-templasttick;

    int changeamount = amount-intolasttick;

    Logger::Get()->Info("Engine: changing tick counter by "+Convert::ToString(changeamount));
        
    LastFrame += changeamount;
}
// ------------------------------------ //
int TestCrash(int writenum){

    int* target = nullptr;
    (*target) = writenum;
    
    Logger::Get()->Write("It didnt' crash...");
    return 42;
}

DLLEXPORT void Leviathan::Engine::PassCommandLine(const wstring &commands){

	Logger::Get()->Info(L"Command line: "+commands);

	GUARD_LOCK_THIS_OBJECT();
	// Split all flags and check for some flags that might be set //
	StringIterator itr(commands);
	unique_ptr<wstring> splitval;

	while((splitval = itr.GetNextCharacterSequence<wstring>(UNNORMALCHARACTER_TYPE_WHITESPACE)) != NULL){

		if(*splitval == L"--nogui"){
			NoGui = true;
			Logger::Get()->Info(L"Engine starting in non-GUI mode");
			continue;
		}
		if(*splitval == L"--noleap"){
			NoLeap = true;

			Logger::Get()->Info(L"Engine starting with LeapMotion disabled");
			continue;
		}
		if(*splitval == L"--nonothing"){
			// Shouldn't try to open the console on windows //
			DEBUG_BREAK;
		}
        if(*splitval == L"--crash"){
            // Test crashing //
            TestCrash(12);
            
            Logger::Get()->Info("Engine testing crash handling");
            // TODO: write a file that disables crash handling
            // Make the log say something useful //
            Logger::Get()->Save();
            continue;
        }
		// Add (if not processed already) //
		PassedCommands.push_back(move(splitval));
	}
}

DLLEXPORT void Leviathan::Engine::ExecuteCommandLine(){
	GUARD_LOCK_THIS_OBJECT();

	StringIterator itr(NULL, false);

	// Iterate over the commands and process them //
	for(size_t i = 0; i < PassedCommands.size(); i++){
		itr.ReInit(PassedCommands[i].get());
		// Skip the preceding '-'s //
		itr.SkipCharacters(L'-');

		// Get the command //
		auto firstpart = itr.GetUntilNextCharacterOrAll<wstring>(L':');

		// Execute the wanted command //
		if(StringOperations::CompareInsensitive<wstring>(*firstpart, L"RemoteConsole")){
			
			// Get the next command //
			auto commandpart = itr.GetUntilNextCharacterOrAll<wstring>(L':');

			if(*commandpart == L"CloseIfNone"){
				// Set the command //
				RemoteConsole::Get()->SetCloseIfNoRemoteConsole(true);
				Logger::Get()->Info(L"Engine will close when no active/waiting remote console sessions");

			} else if(*commandpart == L"OpenTo"){
				// Get the to part //
				auto topart = itr.GetStringInQuotes<wstring>(QUOTETYPE_BOTH);

				int token = 0;

				auto numberpart = itr.GetNextNumber<wstring>(DECIMALSEPARATORTYPE_NONE);

				if(numberpart->size() == 0){

					Logger::Get()->Warning(L"Engine: ExecuteCommandLine: RemoteConsole: no token number provided");
					continue;
				}
				// Convert to a real number. Maybe we could see if the token is complex enough here,
                // but that isn't necessary
				token = Convert::WstringToInt(*numberpart);

				if(token == 0){
					// Invalid number? //
					Logger::Get()->Warning(L"Engine: ExecuteCommandLine: RemoteConsole: couldn't parse token number, "+
                        *numberpart);
					continue;
				}

				// Create a connection (or potentially use an existing one) //
				shared_ptr<ConnectionInfo> tmpconnection = NetworkHandler::Get()->GetOrCreatePointerToConnection(
                    *topart);

				// Tell remote console to open a command to it //
				if(tmpconnection){

					RemoteConsole::Get()->OfferConnectionTo(tmpconnection.get(), L"AutoOpen", token);

				} else {
					// Something funky happened... //
					Logger::Get()->Warning(L"Engine: ExecuteCommandLine: RemoteConsole: couldn't open connection to "+
                        *topart+L", couldn't resolve address");
				}

			} else {
				// Unknown command //
				Logger::Get()->Warning(L"Engine: ExecuteCommandLine: unknown RemoteConsole command: "+*commandpart+
                    L", whole argument: "+*PassedCommands[i]);
			}
		}

        

	}


	PassedCommands.clear();


	// Now we can set some things that require command line arguments //
	// _RemoteConsole might be NULL //
	if(_RemoteConsole)
		_RemoteConsole->SetAllowClose();

}
// ------------------------------------ //
#ifdef _WIN32
DLLEXPORT void Leviathan::Engine::WinAllocateConsole(){
	// Method as used on http://dslweb.nwnexus.com/~ast/dload/guicon.htm //
	int hConHandle;

	long lStdHandle;

	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	FILE *fp;

	// Allocate a console for this program //
	AllocConsole();

	// Set the wanted size //
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),	&coninfo);

	coninfo.dwSize.Y = MAX_CONSOLE_LINES;

	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),	coninfo.dwSize);

	// Redirect STDOUT to the console //
	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen(hConHandle, "w");

	*stdout = *fp;

	setvbuf(stdout, NULL, _IONBF, 0);

	// Now redirect STDIN to the console //
	lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "r" );

	*stdin = *fp;

	setvbuf(stdin, NULL, _IONBF, 0);

	// And finally STDERR to the console //
	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);

	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);

	fp = _fdopen( hConHandle, "w" );

	*stderr = *fp;

	setvbuf(stderr, NULL, _IONBF, 0);

	// Make std library output functions output to console, too //
	ios::sync_with_stdio();
}

DLLEXPORT void Leviathan::Engine::DumpMemoryLeaks(){
#ifdef _DEBUG
#ifdef LEVIATHAN_USES_VLD

	VLDReportLeaks();

#endif // LEVIATHAN_USES_VLD
#endif // _DEBUG
}

#endif
