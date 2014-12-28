#ifndef PONG_COMMON
#define PONG_COMMON
// ------------------------------------ //
#ifndef LEVIATHAN_DEFINE
#include "Define.h"
#endif
// ------------------------------------ //
// ---- includes ---- //
#include "Arena.h"
#include "PlayerSlot.h"
#include "PongConstraints.h"
#include "Entities/Bases/BasePhysicsObject.h"
#include "Utility/DataHandling/SimpleDatabase.h"
#include "Entities/Objects/ViewerCameraPos.h"
#include "Entities/GameWorld.h"
#include "Entities/Objects/Prop.h"
#include "Script/ScriptExecutor.h"
#include "Addons/GameModule.h"
#include "Threading/QueuedTask.h"
#include "add_on/autowrapper/aswrappedcall.h"
#include "Application/GameConfiguration.h"
#include "Application/Application.h"
#include "PongPackets.h"
#include "Newton/PhysicalMaterial.h"
#include "Newton/PhysicsMaterialManager.h"
#include "Networking/SyncedResource.h"
#include "GameInputController.h"
#include "Events/EventHandler.h"
#include "Threading/ThreadingManager.h"

#define SCRIPT_REGISTERFAIL	Logger::Get()->Error(L"PongGame: AngelScript: register global failed in file " +__WFILE__+ L" on line "+Convert::IntToWstring(__LINE__), false);return false;

#define BALLSTUCK_THRESHOLD		0.045f
#define BALLSTUCK_COUNT			8
#define SCOREPOINT_AMOUNT		1

namespace Pong{

	class BasePongParts;
	//! \brief Should be in BasePongParts, used for static access
	//!
	//! Why is gcc so stupid on linux that it does not allow __declspec(selectany)
	extern BasePongParts* BasepongStaticAccess;

	//! \brief A parent class for the CommonPongParts class to allow non-template use
	//!
	//! Mainly required for passing CommonPongParts to non-template functions
	class BasePongParts{
		friend Arena;
	public:

		static void StatUpdater(PlayerList* list){
			Get()->OnPlayerStatsUpdated(list);
		}


		BasePongParts(bool isserver) :
            GameArena(nullptr), ErrorState("No error"),  
			ScoreLimit(L"ScoreLimit", 20),
			GameConfigurationData(new Leviathan::SimpleDatabase(L"GameConfiguration")),
			GamePaused(L"GamePaused", false), GameAI(NULL),
            LastPlayerHitBallID(L"LastPlayerHitBallID", -1),
            _PlayerList(boost::function<void (PlayerList*)>(&StatUpdater), 4)
		{
			BasepongStaticAccess = this;
		}

		~BasePongParts(){

			SAFE_DELETE(GameAI);
			BasepongStaticAccess = NULL;
		}

		//! \brief Updates the ball trail based on the player colour
		void SetBallLastHitColour(){
			// Find the player with the last hit identifier and apply that player's colour //
			auto tmplist = _PlayerList.GetVec();

			for(size_t i = 0; i < tmplist.size(); i++){

				PlayerSlot* slotptr = tmplist[i];

				while(slotptr){

					if(LastPlayerHitBallID == slotptr->GetPlayerNumber()){
						// Set colour //
						GameArena->ColourTheBallTrail(slotptr->GetColour());
						return;
					}

					slotptr = slotptr->GetSplit();
				}
			}
			
			// No other colour is applied so set the default colour //
			GameArena->ColourTheBallTrail(Float4(1.f, 1.f, 1.f, 1.f));
		}


		//! \brief posts a quit message to quit after script has returned
		void ScriptCloseGame(){

			Leviathan::LeviathanApplication::GetApp()->MarkAsClosing();
		}


		//! \brief Will determine if a paddle could theoretically hit the ball
		bool IsBallInGoalArea(){
			// Tell arena to handle this //
			return GameArena->IsBallInPaddleArea();
		}

        //! \brief Called before closing the engine, use to release some stuff
        virtual void CustomEnginePreShutdown(){

            
        }



		bool PlayerIDMatchesGoalAreaID(int plyid, Leviathan::BasePhysicsObject* goalptr){
			// Look through all players and compare find the right PlayerID and compare goal area ptr //
			for(size_t i = 0; i < _PlayerList.Size(); i++){

				PlayerSlot* slotptr = _PlayerList[i];

				while(slotptr){

					if(plyid == slotptr->GetPlayerNumber()){
						// Check if goal area matches //
						Leviathan::BasePhysicsObject* tmpptr = dynamic_cast<Leviathan::BasePhysicsObject*>(
                            slotptr->GetGoalArea().get());
                        
						if(tmpptr == goalptr){
							// Found matching goal area //
							return true;
						}
					}

					slotptr = slotptr->GetSplit();
				}
			}
			// Not found //
			return false;
		}


		//! \warning increases reference count
		Leviathan::Entity::Prop* GetBall(){
			auto tmp = GameArena->GetBallPtr();
            if(!tmp)
                return NULL;
			tmp->AddRef();
			return dynamic_cast<Leviathan::Entity::Prop*>(tmp.get());
		}

		Leviathan::SimpleDatabase* GetGameDatabase(){
			return GameConfigurationData.get();
		}
		Leviathan::GameWorld* GetGameWorld(){
			return WorldOfPong.get();
		}
		int GetLastHitPlayer(){
			return LastPlayerHitBallID;
		}

		// Variable set/get //
		PlayerSlot* GetPlayerSlot(int id){
			return _PlayerList[id];
		}

		void inline SetError(const string &error){
			ErrorState = error;
		}
		string GetErrorString(){
			return ErrorState;
		}

		int GetScoreLimit(){
			return ScoreLimit;
		}

		BaseNotifiableAll* GetPlayersAsNotifiable(){

			return static_cast<BaseNotifiableAll*>(&_PlayerList);
		}

		static BasePongParts* Get(){
			return BasepongStaticAccess;
		}

		PlayerList* GetPlayers(){

			return &_PlayerList;
		}

		static PongInputFactory* GetInputFactory(){
			return InputFactory;
		}

	protected:


		// These should be overridden by the child class //
		virtual void DoSpecialPostLoad() = 0;
		virtual void CustomizedGameEnd() = 0;
		virtual void OnPlayerStatsUpdated(PlayerList* list) = 0;

		// ------------------------------------ //

		// game objects //
		unique_ptr<Arena> GameArena;
		shared_ptr<GameWorld> WorldOfPong;

		// AI module //
		GameModule* GameAI;

		SyncedPrimitive<int> LastPlayerHitBallID;

		SyncedPrimitive<bool> GamePaused;
		SyncedPrimitive<int> ScoreLimit;

		// Configuration data //
		shared_ptr<Leviathan::SimpleDatabase> GameConfigurationData;

		PlayerList _PlayerList;

		//! stores last error string for easy access from scripts
		string ErrorState;


		static PongInputFactory* InputFactory;
	};



	//! \brief Class that contains common functions required both by Pong and PongServer
	//!
	//! Set the program type to match the proper class LeviathanApplication or ServerApplication and the IsServer bool
	//! to match it (to make syncing with the server work)
	template<class ProgramType, bool IsServer>
	class CommonPongParts : public BasePongParts, public ProgramType{
	public:
		CommonPongParts() : BasePongParts(IsServer)
		{

		}
		~CommonPongParts(){

		}


        virtual shared_ptr<GameWorld> GetGameWorld(int id) override{
            // The IDs won't probably match, so return our only world anyways //
            // if(!id != WorldOfPong->GetID()){

            //     Logger::Get()->Error("Pong asked to return a world that isn't WorldOfPong, ID: "+
            //         Convert::ToString(id)+", WorldOfPong: "+Convert::ToString(WorldOfPong->GetID()));
            //     return nullptr;
            // }

            return WorldOfPong;
        }

		// These handle the common code between the server and client //
		virtual void CustomizeEnginePostLoad(){
			using namespace Leviathan;

			QUICKTIME_THISSCOPE;

			Engine::Get()->GetThreadingManager()->QueueTask(shared_ptr<QueuedTask>(new QueuedTask(boost::bind<void>([](
				shared_ptr<Leviathan::SimpleDatabase> GameConfigurationData) -> void
			{

				wstring savefile;

				GAMECONFIGURATION_GET_VARIABLEACCESS(vars);

				assert(vars->GetValueAndConvertTo<wstring>(L"GameDatabase", savefile) &&
                    "invalid game variable configuration, no GameDatabase");

				GameConfigurationData->LoadFromFile(savefile);
				Logger::Get()->Info(L"Loaded game configuration database");

			}, GameConfigurationData))));

			Engine::Get()->GetThreadingManager()->QueueTask(shared_ptr<QueuedTask>(new QueuedTask(boost::bind<void>([](
                                CommonPongParts* game) -> void
                {
                    // Load the game AI //
                    game->GameAI = new GameModule(L"PongAIModule", L"PongGameCore");

                    if(!game->GameAI->Init()){
                        // No AI for the game //
                        Logger::Get()->Error(L"Failed to load AI!");
                        SAFE_DELETE(game->GameAI);
                    }

                }, this))));


			Engine::Get()->GetThreadingManager()->QueueTask(shared_ptr<QueuedTask>(new QueuedTask(boost::bind<void>([]()
                            -> void
                {
                    // Load Pong specific packets //
                    PongPackets::RegisterAllPongPacketTypes();
                    PongConstraintSerializer::Register();            
                    Logger::Get()->Info(L"Pong specific packets loaded");
                    
                }))));

			// Setup world //
			WorldOfPong = Engine::GetEngine()->CreateWorld(Engine::Get()->GetWindowEntity(), NULL);

			// create playing field manager with the world //
			GameArena = unique_ptr<Arena>(new Arena(WorldOfPong));

			DoSpecialPostLoad();

			// Register the variables //
			ScoreLimit.StartSync();
			GamePaused.StartSync();
			_PlayerList.StartSync();
            LastPlayerHitBallID.StartSync();

			// Wait for everything to finish //
			Engine::Get()->GetThreadingManager()->WaitForAllTasksToFinish();
		}

		virtual void EnginePreShutdown() override{
			// Only the AI needs this //
			if(GameAI)
				GameAI->ReleaseScript();

            // Destroy the world //
            Engine::Get()->DestroyWorld(WorldOfPong);
            
            CustomEnginePreShutdown();

            Logger::Get()->Info("Pong PreShutdown complete");
		}

        //! Override this
		virtual void Tick(int mspassed) override{

            DEBUG_BREAK;
		}

		// customized callbacks //
		virtual bool InitLoadCustomScriptTypes(asIScriptEngine* engine) override{

			// register PongGame type //
			if(engine->RegisterObjectType("PongBase", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
				SCRIPT_REGISTERFAIL;
			}

			// get function //
			if(engine->RegisterGlobalFunction("PongBase@ GetPongBase()", WRAP_FN(&BasePongParts::Get), asCALL_GENERIC) < 0){
				SCRIPT_REGISTERFAIL;
			}

			// functions //
			if(engine->RegisterObjectMethod("PongBase", "void Quit()", WRAP_MFN(BasePongParts, ScriptCloseGame), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PongBase", "string GetErrorString()", WRAP_MFN(BasePongParts, GetErrorString), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PongBase", "int GetLastHitPlayer()", WRAP_MFN(BasePongParts, GetLastHitPlayer), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			// For getting the game database //
			if(engine->RegisterObjectMethod("PongBase", "SimpleDatabase& GetGameDatabase()", WRAP_MFN(BasePongParts, GetGameDatabase), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PongBase", "GameWorld& GetGameWorld()", WRAP_MFN(BasePongParts, GetGameWorld), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PongBase", "Prop@ GetBall()", WRAP_MFN(BasePongParts, GetBall), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PongBase", "BaseNotifiableAll& GetPlayerChanges()", asMETHOD(BasePongParts, GetPlayersAsNotifiable), asCALL_THISCALL) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}



			// Type enums //
			if(engine->RegisterEnum("PLAYERTYPE") < 0){
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERTYPE", "PLAYERTYPE_CLOSED", PLAYERTYPE_CLOSED) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERTYPE", "PLAYERTYPE_COMPUTER", PLAYERTYPE_COMPUTER) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERTYPE", "PLAYERTYPE_EMPTY", PLAYERTYPE_EMPTY) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERTYPE", "PLAYERTYPE_HUMAN", PLAYERTYPE_HUMAN) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterEnum("PLAYERCONTROLS") < 0){
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_NONE", PLAYERCONTROLS_NONE) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_AI", PLAYERCONTROLS_AI) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_WASD", PLAYERCONTROLS_WASD) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_ARROWS", PLAYERCONTROLS_ARROWS) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_IJKL", PLAYERCONTROLS_IJKL) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_NUMPAD", PLAYERCONTROLS_NUMPAD) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("PLAYERCONTROLS", "PLAYERCONTROLS_CONTROLLER", PLAYERCONTROLS_CONTROLLER) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}


			if(engine->RegisterEnum("CONTROLKEYACTION") < 0){
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("CONTROLKEYACTION", "CONTROLKEYACTION_LEFT", CONTROLKEYACTION_LEFT) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("CONTROLKEYACTION", "CONTROLKEYACTION_RIGHT", CONTROLKEYACTION_RIGHT) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("CONTROLKEYACTION", "CONTROLKEYACTION_POWERUPDOWN", CONTROLKEYACTION_POWERUPDOWN) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterEnumValue("CONTROLKEYACTION", "CONTROLKEYACTION_POWERUPUP", CONTROLKEYACTION_POWERUPUP) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			// PlayerSlot //
			if(engine->RegisterObjectType("PlayerSlot", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
				SCRIPT_REGISTERFAIL;
			}

			// get function //
			if(engine->RegisterObjectMethod("PongBase", "PlayerSlot@ GetSlot(int number)", WRAP_MFN(BasePongParts, GetPlayerSlot), asCALL_GENERIC) < 0){
				SCRIPT_REGISTERFAIL;
			}

			// functions //
			if(engine->RegisterObjectMethod("PlayerSlot", "bool IsActive()", WRAP_MFN(PlayerSlot, IsSlotActive), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PlayerSlot", "PLAYERTYPE GetPlayerType()", asMETHOD(PlayerSlot, GetPlayerType), asCALL_THISCALL) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			

			if(engine->RegisterObjectMethod("PlayerSlot", "int GetPlayerNumber()", WRAP_MFN(PlayerSlot, GetPlayerNumber), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PlayerSlot", "int GetScore()", WRAP_MFN(PlayerSlot, GetScore), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			if(engine->RegisterObjectMethod("PlayerSlot", "PlayerSlot@ GetSplit()", WRAP_MFN(PlayerSlot, GetSplit), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "PLAYERCONTROLS GetControlType()", WRAP_MFN(PlayerSlot, GetControlType), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "void AddEmptySubSlot()", WRAP_MFN(PlayerSlot, AddEmptySubSlot), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "void SetControls(PLAYERCONTROLS type, int identifier)", WRAP_MFN(PlayerSlot, SetControls), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "void SetPlayerAutoID(PLAYERTYPE type)", WRAP_MFN(PlayerSlot, SetPlayerProxy), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "void PassInputAction(CONTROLKEYACTION actiontoperform, bool active)", WRAP_MFN(PlayerSlot, PassInputAction), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "bool IsVerticalSlot()", WRAP_MFN(PlayerSlot, IsVerticalSlot), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "float GetTrackProgress()", WRAP_MFN(PlayerSlot, GetTrackProgress), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "BaseObject@ GetPaddle()", WRAP_MFN(PlayerSlot, GetPaddleProxy), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "BaseObject@ GetGoalArea()", WRAP_MFN(PlayerSlot, GetGoalAreaProxy), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "TrackEntityController@ GetTrackController()", WRAP_MFN(PlayerSlot, GetTrackController), asCALL_GENERIC) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "bool DoesPlayerNumberMatchThisOrParent(int number)", asMETHOD(PlayerSlot, DoesPlayerNumberMatchThisOrParent), asCALL_THISCALL) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}
			if(engine->RegisterObjectMethod("PlayerSlot", "int GetPlayerID()", asMETHOD(PlayerSlot, GetPlayerID), asCALL_THISCALL) < 0)
			{
				SCRIPT_REGISTERFAIL;
			}

			


			return MoreCustomScriptTypes(engine);
		}

		virtual void RegisterCustomScriptTypes(asIScriptEngine* engine, std::map<int, wstring> &typeids){
			// we have registered just a one type, add it //
			typeids.insert(make_pair(engine->GetTypeIdByDecl("PongBase"), L"PongBase"));
			typeids.insert(make_pair(engine->GetTypeIdByDecl("PlayerSlot"), L"PlayerSlot"));

			MoreCustomScriptRegister(engine, typeids);
		}

		virtual void RegisterApplicationPhysicalMaterials(Leviathan::PhysicsMaterialManager* manager){
			// \todo implement loading from files //

			// load predefined materials //
			unique_ptr<Leviathan::PhysicalMaterial> PaddleMaterial(new Leviathan::PhysicalMaterial(L"PaddleMaterial"));
			unique_ptr<Leviathan::PhysicalMaterial> ArenaMaterial(new Leviathan::PhysicalMaterial(L"ArenaMaterial"));
			unique_ptr<Leviathan::PhysicalMaterial> ArenaBottomMaterial(
                new Leviathan::PhysicalMaterial(L"ArenaBottomMaterial"));
			unique_ptr<Leviathan::PhysicalMaterial> BallMaterial(new Leviathan::PhysicalMaterial(L"BallMaterial"));
			unique_ptr<Leviathan::PhysicalMaterial> GoalAreaMaterial(
                new Leviathan::PhysicalMaterial(L"GoalAreaMaterial"));

			// Set callbacks //
			BallMaterial->FormPairWith(*PaddleMaterial).SetSoftness(1.f).SetElasticity(1.0f).SetFriction(1.f, 1.f).
				SetCallbacks(NULL, GetBallPaddleCallback());
			BallMaterial->FormPairWith(*GoalAreaMaterial).SetCallbacks(NULL, GetBallGoalAreaCallback());

			PaddleMaterial->FormPairWith(*GoalAreaMaterial).SetCollidable(false);
			PaddleMaterial->FormPairWith(*ArenaMaterial).SetCollidable(false).SetElasticity(0.f).SetSoftness(0.f);
			PaddleMaterial->FormPairWith(*ArenaBottomMaterial).SetCollidable(false).SetSoftness(0.f).SetFriction(0.f, 0.f).SetElasticity(0.f);
			PaddleMaterial->FormPairWith(*PaddleMaterial).SetCollidable(false);
			ArenaMaterial->FormPairWith(*GoalAreaMaterial).SetCollidable(false);
			ArenaMaterial->FormPairWith(*BallMaterial).SetFriction(0.f, 0.f).SetSoftness(1.f).SetElasticity(1.f);
			ArenaBottomMaterial->FormPairWith(*BallMaterial).SetElasticity(0.f).SetFriction(0.f, 0.f).SetSoftness(0.f);
			ArenaBottomMaterial->FormPairWith(*GoalAreaMaterial).SetCollidable(false);

			// Add the materials //
			Leviathan::PhysicsMaterialManager* tmp = Leviathan::PhysicsMaterialManager::Get();

			tmp->LoadedMaterialAdd(PaddleMaterial.release());
			tmp->LoadedMaterialAdd(ArenaMaterial.release());
			tmp->LoadedMaterialAdd(BallMaterial.release());
			tmp->LoadedMaterialAdd(GoalAreaMaterial.release());
			tmp->LoadedMaterialAdd(ArenaBottomMaterial.release());
		}

        
        
		// ------------------ Physics callbacks for game logic ------------------ //
		// Ball handling callback //

        virtual PhysicsMaterialContactCallback GetBallPaddleCallback() = 0;

        virtual PhysicsMaterialContactCallback GetBallGoalAreaCallback() = 0;
        
	protected:

		virtual bool MoreCustomScriptTypes(asIScriptEngine* engine) = 0;
		virtual void MoreCustomScriptRegister(asIScriptEngine* engine, std::map<int, wstring> &typeids) = 0;

		// ------------------------------------ //

	};

}
#endif
