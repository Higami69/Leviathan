#include "PongIncludes.h"
// ------------------------------------ //
#ifndef PONG_ARENA
#include "Arena.h"
#endif
#include "PongGame.h"
#include "Entities\Objects\Brush.h"
using namespace Pong;
// ------------------------------------ //
Pong::Arena::Arena(shared_ptr<Leviathan::GameWorld> world) : TargetWorld(world){

}

Pong::Arena::~Arena(){

}
// ------------------------------------ //
bool Pong::Arena::GenerateArena(PongGame* game, std::vector<PlayerSlot*> &players, int plycount, int maximumsplit, bool clearfirst /*= true*/){
	// check sanity of values //
	QUICKTIME_THISSCOPE;

	if(plycount == 0 || plycount > 4){
		game->SetError("Player count must be between 1 and 4");
		return false;
	}

	if(maximumsplit > 2){
		game->SetError("Sides have to be split into two or be whole (max 2 players per side)");
		return false;
	}

	if(clearfirst){
		TargetWorld->ClearObjects();
		_ClearPointers();
	}

	// calculate sizes //
	float width = 20*BASE_ARENASCALE;
	float height = width;

	float mheight = 3*BASE_ARENASCALE;
	float sideheight = 2*BASE_ARENASCALE;
	float paddleheight = 1*BASE_ARENASCALE;
	float paddlewidth = 3*BASE_ARENASCALE;
	float bottomthickness = 0.5*BASE_ARENASCALE;

	float paddlethickness = maximumsplit == 0 ? 1*BASE_ARENASCALE: 0.5f*BASE_ARENASCALE;

	float sidexsize = width/20.f;
	float sideysize = height/20.f;

	float paddlemass = 60.f;
	//float paddlemass = 0.f;

	string materialbase = "Material.001";
	string materialpaddle = "BaseWhite";
	string sidematerialtall = "Material.001";
	string sidematerialshort = "Material.001";
	string materialclosedpaddlearea = "Material.001";

	// create brushes //

	Leviathan::ObjectLoader* loader = Engine::GetEngine()->GetObjectLoader();

	// WARNING: Huge mess ahead!
	// GetWorldObject is used because the ptr returned by load is not "safe" to use, so we get a shared ptr to the same object, this avoids dynamic
	// cast and is safe at the same time

	// base surface brush //
	Leviathan::Entity::Brush* castedbottombrush;
	BottomBrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), materialbase, Float3(width, bottomthickness, height), 0.f,
		&castedbottombrush));
	castedbottombrush->SetPos(0.f, -bottomthickness/2.f, 0.f);
	
	// arena sides //

	// left top //
	Leviathan::Entity::Brush* tmp;
	auto tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialtall, Float3(sidexsize, mheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize/2.f, mheight/2.f, -height/2.f+sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize*1.5f, sideheight/2.f, -height/2.f+sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize),
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize/2.f, sideheight/2.f, -height/2.f+sideysize*1.5f);
	

	// top right //
	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialtall, Float3(sidexsize, mheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize/2.f, mheight/2.f, -height/2.f+sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize*1.5f, sideheight/2.f, -height/2.f+sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize/2.f, sideheight/2.f, -height/2.f+sideysize*1.5f);
	


	// bottom left //
	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialtall, Float3(sidexsize, mheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize/2.f, mheight/2.f, height/2.f-sideysize/2.f);

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize*1.5f, sideheight/2.f, height/2.f-sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(-width/2.f+sidexsize/2.f, sideheight/2.f, height/2.f-sideysize*1.5f);
	

	// bottom right //
	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialtall, Float3(sidexsize, mheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize/2.f, mheight/2.f, height/2.f-sideysize/2.f);
	

	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize*1.5f, sideheight/2.f, height/2.f-sideysize/2.f);


	tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, Float3(sidexsize, sideheight, sideysize), 
		0.f, &tmp));
	tmp->SetPos(width/2.f-sidexsize/2.f, sideheight/2.f, height/2.f-sideysize*1.5f);


	// fill empty paddle spaces //
	if(plycount < 2){

		// fill left with wall //
		tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, 
			Float3(sidexsize, sideheight/2, sideysize*16.f), 0.f, &tmp));
		tmp->SetPos(-width/2.f+sidexsize/2.f, sideheight/4.f, 0);
	}

	if(plycount < 3){

		// fill bottom with wall //
		tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, 
			Float3(sidexsize*16.f, sideheight/2, sideysize), 0.f, &tmp));
		tmp->SetPos(0, sideheight/4.f, height/2.f-sideysize/2.f);
	}
	if(plycount < 4){

		// fill top with wall //
		tmpbrush = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), sidematerialshort, 
			Float3(sidexsize*16.f, sideheight/2, sideysize), 0.f, &tmp));
		tmp->SetPos(0, sideheight/4.f, -height/2.f+sideysize/2.f);
	}


	// paddles and link slots to objects//

	// loop through players and add paddles //
	for(size_t i = 0; i < players.size(); i++){
		// skip empty slots //
		if(!players[i]->IsSlotActive())
			continue;
		bool secondary = false;
addplayerpaddlelabel:

		// add paddle based on loop index //
		auto plypaddle = TargetWorld->GetWorldObject(loader->LoadBrushToWorld(TargetWorld.get(), materialpaddle, 
			Float3((i == 0 || i == 2) ? paddlethickness: paddlewidth, paddleheight, (i == 0 || i == 2) ? paddlewidth: paddlethickness), paddlemass,
			&tmp));
		// setup position //
		float horiadjust = 0;
		if(maximumsplit >= 1)
			horiadjust = secondary ? -paddlethickness: paddlethickness;

		switch(i){
		case 0: tmp->SetPos(width/2.f-paddlethickness/2.f-horiadjust, paddleheight/2.f, 0); break;
		case 1: tmp->SetPos(0, paddleheight/2.f, width/2.f-paddlethickness/2.f-horiadjust); break;
		case 2: tmp->SetPos(-width/2.f+paddlethickness/2.f+horiadjust, paddleheight/2.f, 0);break;
		case 3: tmp->SetPos(0, paddleheight/2.f, -width/2.f+paddlethickness/2.f+horiadjust); break;
		}

		// setup joints //
		if(!tmp->CreateConstraintWith<Leviathan::Entity::SliderConstraint>(castedbottombrush)->SetParameters(
			(i == 0 || i == 2) ? Float3(0.f, 0.f, -1.f): Float3(1.f, 0.f, 0.f))->Init()){
			Logger::Get()->Error(L"Arena: GenerateArena: failed to create slider for paddle "+Convert::ToWstring(i+1));
		}

		// link //
		secondary ? players[i]->GetSplit()->SetPaddleObject(plypaddle): players[i]->SetPaddleObject(plypaddle);

		if(secondary)
			continue;
		// loop again if has secondary //
		if(players[i]->GetSplit() != NULL){
			secondary = true;
			goto addplayerpaddlelabel;
		}
	}


	return true;
}
// ------------------------------------ //
void Pong::Arena::_ClearPointers(){
	BottomBrush.reset();
}
// ------------------------------------ //

// ------------------------------------ //

