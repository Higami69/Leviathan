// ------------------------------------ //
#ifndef LEVIATHAN_BASE_POSITIONABLE
#include "BasePositionable.h"
#endif
#include "BaseObject.h"
#include "Exceptions.h"
#include "../CommonStateObjects.h"
using namespace Leviathan;
// ------------------------------------ //
BasePositionable::BasePositionable() : QuatRotation(Float4::IdentityQuaternion()), Position(Float3(0)){

}

DLLEXPORT Leviathan::BasePositionable::BasePositionable(const Float3 &pos, const Float4 &orientation) :
    Position(pos), QuatRotation(orientation)
{

}

BasePositionable::~BasePositionable(){

}
// ------------------------------------ //
void BasePositionable::SetPosX(const float &x){
    GUARD_LOCK_THIS_OBJECT();
	Position.X = x;
	PosUpdated();
}
void BasePositionable::SetPosY(const float &y){
    GUARD_LOCK_THIS_OBJECT();
	Position.Y = y;
	PosUpdated();
}
void BasePositionable::SetPosZ(const float &z){
    GUARD_LOCK_THIS_OBJECT();
	Position.Z = z;
	PosUpdated();
}
// ------------------------------------ //
Float4 Leviathan::BasePositionable::GetOrientation() const{
    GUARD_LOCK_THIS_OBJECT();
	return QuatRotation;
}

void Leviathan::BasePositionable::GetOrientation(Float4 &receiver) const{
    GUARD_LOCK_THIS_OBJECT();
	receiver = QuatRotation;
}

DLLEXPORT void Leviathan::BasePositionable::GetRotation(Float4 &receiver) const{
    GUARD_LOCK_THIS_OBJECT();
	receiver = QuatRotation;
}

DLLEXPORT Float4 BasePositionable::GetRotation() const{
    GUARD_LOCK_THIS_OBJECT();
    return QuatRotation;
}
// ------------------------------------ //
void BasePositionable::GetPosElements(float &outx, float &outy, float &outz){
    GUARD_LOCK_THIS_OBJECT();
	outx = Position.X;
	outy = Position.Y;
	outz = Position.Z;
}

DLLEXPORT Float3 Leviathan::BasePositionable::GetPos() const{
    GUARD_LOCK_THIS_OBJECT();
	return Position;
}

DLLEXPORT void Leviathan::BasePositionable::GetPos(Float3 &receiver) const{
    GUARD_LOCK_THIS_OBJECT();
    receiver = Position;
}

DLLEXPORT Float3 BasePositionable::GetPosition() const{
    GUARD_LOCK_THIS_OBJECT();
    return Position;
}

float BasePositionable::GetPosX() const{
    GUARD_LOCK_THIS_OBJECT();
	return Position.X;
}
float BasePositionable::GetPosY() const{
    GUARD_LOCK_THIS_OBJECT();
	return Position.Y;
}
float BasePositionable::GetPosZ() const{
    GUARD_LOCK_THIS_OBJECT();
	return Position.Z;
}

DLLEXPORT void Leviathan::BasePositionable::SetPosComponents(const float &x, const float &y, const float &z){
    GUARD_LOCK_THIS_OBJECT();
	Position.X = x;
	Position.Y = y;
	Position.Z = z;
	PosUpdated();
}

DLLEXPORT void Leviathan::BasePositionable::SetPos(const Float3 &pos){
    GUARD_LOCK_THIS_OBJECT();
	Position = pos;
	PosUpdated();
}

DLLEXPORT void Leviathan::BasePositionable::SetPosition(const Float3 &pos){
    GUARD_LOCK_THIS_OBJECT();
	Position = pos;
	PosUpdated();
}

DLLEXPORT void Leviathan::BasePositionable::SetOrientation(const Float4 &quat){
    GUARD_LOCK_THIS_OBJECT();
	QuatRotation = quat;
	OrientationUpdated();
}

DLLEXPORT void Leviathan::BasePositionable::SetOrientationComponents(const float &x, const float &y,
    const float &z, const float &w)
{
    GUARD_LOCK_THIS_OBJECT();
    QuatRotation = Float4(x, y, z, w);
    OrientationUpdated();
}

void Leviathan::BasePositionable::PosUpdated(){

}

void Leviathan::BasePositionable::OrientationUpdated(){

}
// ------------------------------------ //
DLLEXPORT void Leviathan::BasePositionable::ApplyPositionDataObject(const BasePositionData &pos){

    GUARD_LOCK_THIS_OBJECT();
    Position = pos.Position;
    PosUpdated();

    QuatRotation = pos.QuatRotation;
    OrientationUpdated();
}
// ------------------------------------ //
bool Leviathan::BasePositionable::BasePositionableCustomMessage(int message, void* data){
	switch(message){
        case ENTITYCUSTOMMESSAGETYPE_CHANGEWORLDPOSITION:
        {
            Position = *reinterpret_cast<Float3*>(data); PosUpdated(); return true;
        }


	}
	return false;
}

bool Leviathan::BasePositionable::BasePositionableCustomGetData(ObjectDataRequest* data){

	switch(data->RequestObjectPart){
	case ENTITYDATA_REQUESTTYPE_WORLDPOSITION: data->RequestResult = &Position; return true;

	}

	return false;
}
// ------------------------------------ //
DLLEXPORT void Leviathan::BasePositionable::AddPositionAndRotationToPacket(sf::Packet &packet){
    GUARD_LOCK_THIS_OBJECT();
    packet << Position;
    packet << QuatRotation;
}

DLLEXPORT void Leviathan::BasePositionable::ApplyPositionAndRotationFromPacket(sf::Packet &packet){

    // First get the data //
    Float4 quaternion;
    Float3 pos;
    
    packet >> pos;
    packet >> quaternion;
    
    // Don't apply if any of the reads have failed //
    if(!packet){
        // Some read has failed //
        throw InvalidArgument("invalid packet");
    }

    // Apply the data //
    SetPos(pos);
    SetOrientation(quaternion);
}

DLLEXPORT bool Leviathan::BasePositionable::LoadPositionFromPacketToHolder(sf::Packet &packet,
    BasePositionData &target)
{
    packet >> target.Position;
    packet >> target.QuatRotation;
    
    return packet ? true: false;
}
// ------------------------------------ //
DLLEXPORT void BasePositionable::InterpolatePositionableState(PositionableRotationableDeltaState &first,
    PositionableRotationableDeltaState &second, float progress)
{
    
    Float3 pos = GetPosition();
    Float4 rot = GetOrientation();

    // Position
    if(second.ValidFields & PRDELTAUPDATED_POS_X){

        if(first.ValidFields & PRDELTAUPDATED_POS_X){
            
            pos.X = first.Position.X + (second.Position.X-first.Position.X)*progress;
        } else {

            pos.X = second.Position.X;
        }
        
    } else if(first.ValidFields &PRDELTAUPDATED_POS_X){
        
        pos.X = first.Position.X;
    }

    if(second.ValidFields & PRDELTAUPDATED_POS_Y){

        if(first.ValidFields & PRDELTAUPDATED_POS_Y){
            
            pos.Y = first.Position.Y + (second.Position.Y-first.Position.Y)*progress;
        } else {

            pos.Y = second.Position.Y;
        }
        
    } else if(first.ValidFields &PRDELTAUPDATED_POS_Y){
        
        pos.Y = first.Position.Y;
    }
    
    if(second.ValidFields & PRDELTAUPDATED_POS_Z){

        if(first.ValidFields & PRDELTAUPDATED_POS_Z){
            
            pos.Z = first.Position.Z + (second.Position.Z-first.Position.Z)*progress;
        } else {

            pos.Z = second.Position.Z;
        }
        
    } else if(first.ValidFields &PRDELTAUPDATED_POS_Z){
        
        pos.Z = first.Position.Z;
    }
    
    // Rotation
    // TODO: spherical interpolation for rotation
    if(second.ValidFields & PRDELTAUPDATED_ROT_X){

        if(first.ValidFields & PRDELTAUPDATED_ROT_X){
            
            rot.X = first.Rotation.X + (second.Rotation.X-first.Rotation.X)*progress;
        } else {

            rot.X = second.Rotation.X;
        }
        
    } else if(first.ValidFields & PRDELTAUPDATED_ROT_X){

        rot.X = first.Rotation.X;
    }

    if(second.ValidFields & PRDELTAUPDATED_ROT_Y){

        if(first.ValidFields & PRDELTAUPDATED_ROT_Y){
            
            rot.Y = first.Rotation.Y + (second.Rotation.Y-first.Rotation.Y)*progress;
        } else {

            rot.Y = second.Rotation.Y;
        }
        
    } else if(first.ValidFields & PRDELTAUPDATED_ROT_Y){

        rot.Y = first.Rotation.Y;
    }
    
    if(second.ValidFields & PRDELTAUPDATED_ROT_Z){

        if(first.ValidFields & PRDELTAUPDATED_ROT_Z){
            
            rot.Z = first.Rotation.Z + (second.Rotation.Z-first.Rotation.Z)*progress;
        } else {

            rot.Z = second.Rotation.Z;
        }
        
    } else if(first.ValidFields & PRDELTAUPDATED_ROT_Z){

        rot.Z = first.Rotation.Z;
    }
    
    if(second.ValidFields & PRDELTAUPDATED_ROT_W){

        if(first.ValidFields & PRDELTAUPDATED_ROT_W){
            
            rot.W = first.Rotation.W + (second.Rotation.W-first.Rotation.W)*progress;
        } else {

            rot.W = second.Rotation.W;
        }
        
    } else if(first.ValidFields & PRDELTAUPDATED_ROT_W){

        rot.W = first.Rotation.W;
    }
    
    GUARD_LOCK_THIS_OBJECT();
    SetPos(pos);
    SetOrientation(rot);
}
