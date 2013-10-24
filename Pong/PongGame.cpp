#include "PongIncludes.h"
// ------------------------------------ //
#ifndef PONG_GAME
#include "PongGame.h"
#endif
#include "Entities\Objects\ViewerCameraPos.h"
#include "Entities\GameWorld.h"
#include "Entities\Objects\Prop.h"
#include "..\Engine\Script\ScriptExecutor.h"
#include "Arena.h"
using namespace Pong;
// ------------------------------------ //
Pong::PongGame::PongGame() : GameArena(nullptr), ErrorState("No error"), PlayerList(4){
	StaticAccess = this;

	// fill the player list with the player 1 and empty slots //
	PlayerList[0] = new PlayerSlot(0, PLAYERTYPE_HUMAN, 1, PLAYERCONTROLS_WASD, 0);
	// other slots as empty //
	for(size_t i = 1; i < PlayerList.size(); i++){

		PlayerList[i] = new PlayerSlot(i, true);
	}

}

Pong::PongGame::~PongGame(){
	// delete memory //
	SAFE_DELETE_VECTOR(PlayerList);
}
// ------------------------------------ //
void Pong::PongGame::CustomizeEnginePostLoad(){
	// load GUI documents //

	Gui::GuiManager* manager = Engine::GetEngine()->GetWindowEntity()->GetGUI();

	manager->LoadGUIFile(FileSystem::GetScriptsFolder()+L"PongMenus.txt");

#ifdef _DEBUG
	// load debug panel, too //

	manager->LoadGUIFile(FileSystem::GetScriptsFolder()+L"DebugPanel.txt");
#endif // _DEBUG

	manager->SetMouseFile(FileSystem::GetScriptsFolder()+L"cursor.rml");

	// setup world //
	shared_ptr<GameWorld> world1 = Engine::GetEngine()->CreateWorld();

	// set skybox to have some sort of visuals //
	world1->SetSkyBox("NiceDaySky");

	// create playing field manager with the world //
	GameArena = unique_ptr<Arena>(new Arena(world1));

	ObjectLoader* loader = Engine::GetEngine()->GetObjectLoader();


	// camera //
	shared_ptr<ViewerCameraPos> MainCamera(new ViewerCameraPos());
	MainCamera->SetPos(Float3(0.f, 22.f*BASE_ARENASCALE, 0.f));

	// camera should always point down towards the play field //
	MainCamera->SetRotation(Float3(0.f, -90.f, 0.f));



	// link world and camera to a window //
	GraphicalInputEntity* window1 = Engine::GetEngine()->GetWindowEntity();

	window1->LinkCamera(MainCamera);
	window1->LinkWorld(world1);
	// sound listening camera //
	MainCamera->BecomeSoundPerceiver();

	// link window input to camera //
	window1->GetInputController()->LinkReceiver(MainCamera.get());

	// load GUI background //


	// I like the debugger //
	window1->GetGUI()->SetDebuggerOnThisContext();
	//window1->GetGUI()->SetDebuggerVisibility(true);
	
	// after loading reset time sensitive timers //
	Engine::GetEngine()->ResetPhysicsTime();
}

std::wstring Pong::PongGame::GenerateWindowTitle(){
	return wstring(L"Pong version " GAME_VERSIONS L" Leviathan " LEVIATHAN_VERSIONS);
}
// ------------------------------------ //
// TODO: register game objects for use in scripts //
void Pong::PongGame::InitLoadCustomScriptTypes(asIScriptEngine* engine){

	// register PongGame type //
	if(engine->RegisterObjectType("PongGame", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
		SCRIPT_REGISTERFAIL;
	}

	// get function //
	if(engine->RegisterGlobalFunction("PongGame@ GetPongGame()", asFUNCTION(PongGame::Get), asCALL_CDECL) < 0){
		SCRIPT_REGISTERFAIL;
	}

	// functions //
	if(engine->RegisterObjectMethod("PongGame", "int StartGame()", asMETHOD(PongGame, TryStartGame), asCALL_THISCALL) < 0)
	{
		SCRIPT_REGISTERFAIL;
	}

	if(engine->RegisterObjectMethod("PongGame", "void Quit()", asMETHOD(PongGame, ScriptCloseGame), asCALL_THISCALL) < 0)
	{
		SCRIPT_REGISTERFAIL;
	}

	if(engine->RegisterObjectMethod("PongGame", "string GetErrorString()", asMETHOD(PongGame, GetErrorString), asCALL_THISCALL) < 0)
	{
		SCRIPT_REGISTERFAIL;
	}
	
	


	// static functions //

}

void Pong::PongGame::RegisterCustomScriptTypes(asIScriptEngine* engine, std::map<int, wstring> &typeids){
	// we have registered just a one type, add it //
	typeids.insert(make_pair(engine->GetTypeIdByDecl("PongGame"), L"PongGame"));
}
// ------------------------------------ //
PongGame* Pong::PongGame::Get(){
	return StaticAccess;
}

int Pong::PongGame::TryStartGame(){

	int activeplycount = 0;
	int maxsplit = 0;
	for(size_t i = 0; i < PlayerList.size(); i++){
		if(PlayerList[i]->IsSlotActive())
			activeplycount++;
		int split = PlayerList[i]->GetSplitCount();
		if(split > maxsplit)
			maxsplit = split;
	}

	if(!GameArena->GenerateArena(this, PlayerList, activeplycount, maxsplit, true)){

		return -3;
	}

	// TODO: link input //

	// send start event //
	Leviathan::EventHandler::Get()->CallEvent(new Leviathan::GenericEvent(new wstring(L"GameStart"), new NamedVars(shared_ptr<NamedVariableList>(new
		NamedVariableList(L"PlayerCount", new Leviathan::VariableBlock(activeplycount))))));

	// succeeded //
	return 1;
}

void Pong::PongGame::ScriptCloseGame(){
	Engine::GetEngine()->GetWindowEntity()->GetWindow()->SendCloseMessage();
}

string Pong::PongGame::GetErrorString(){
	return ErrorState;
}

PongGame* Pong::PongGame::StaticAccess = NULL;