// Leviathan Game Engine
// Copyright (c) 2012-2016 Henri Hyyryläinen
#pragma once
// ------------------------------------ //
#include "Define.h"
// ------------------------------------ //
#include "Common/SFMLPackets.h"
#include "CommonNetwork.h"
#include "Exceptions.h"

#include "GameSpecificPacketHandler.h"

#include <memory>

namespace Leviathan{

enum class NETWORK_REQUEST_TYPE : uint16_t{
	//! This is sent first, expected result is like
    //! "PongServer running version 0.5.1.0, status: 0/20"
	Identification,
        
	Serverstatus,
        
	RemoteConsoleOpen,
        
	RemoteConsoleAccess,
        
	CloseRemoteConsole,

    //! The receiving side is now allowed to open a remote console with the token 
    DoRemoteConsoleOpen,

    //! Client wants to join a server
    //! MasterServerToken The ID given by the master server
	JoinServer,
        
	GetSingleSyncValue,
        
	GetAllSyncValues,
        
	//! Used to request the server to run a command, used for chat and other things
    //! \todo Implement 	if(Command.length() > MAX_SERVERCOMMAND_LENGTH)
	RequestCommandExecution,
        
	//! Sent when a player requests the server to connect a NetworkedInput
	ConnectInput,
        
    //! Sent by servers to ping (time the time a client takes to respond) clients
    Echo,
        
    //! Contains timing data to sync world clocks on a client
    //! Ticks The amount of ticks to set or change by
    //! Absolute Whether the tick count should be set to be the current or
    //! just added to the current tick
    //!
    //! EngineMSTweak The engine tick tweaking, this should only be
    //! applied by a single GameWorld
    WorldClockSync,

	//! Used for game specific requests
	Custom
};

//! Base class for all request objects
//! \note Even though it cannot be required by the base class, sub classes should
//! implement a constructor taking in an sf::Packet object
class NetworkRequest{
public:

    NetworkRequest(NETWORK_REQUEST_TYPE type, uint32_t idforresponse = 0) :
        Type(type), IDForResponse(idforresponse)
    {
        
    }
    
    virtual ~NetworkRequest(){};

    inline void AddDataToPacket(sf::Packet &packet){

        packet << true << static_cast<uint16_t>(Type);

        _SerializeCustom(packet);
    }

    inline NETWORK_REQUEST_TYPE GetType() const{
        return Type;
    }

    inline uint32_t GetIDForResponse() const {
        return IDForResponse;
    }

    DLLEXPORT static std::shared_ptr<NetworkRequest> LoadFromPacket(sf::Packet &packet, 
        uint32_t packetid);

protected:

    //! \brief Base classes serialize their data
    DLLEXPORT virtual void _SerializeCustom(sf::Packet &packet) = 0;

    const NETWORK_REQUEST_TYPE Type;

    const uint32_t IDForResponse = 0;
};

class RequestCustom : public NetworkRequest{
	public:
    RequestCustom(std::shared_ptr<GameSpecificPacketData> actualrequest) :
        NetworkRequest(NETWORK_REQUEST_TYPE::Custom),
        ActualRequest(actualrequest)
    {}

    void _SerializeCustom(sf::Packet &packet) override{

        LEVIATHAN_ASSERT(0, "_SerializeCustom called on RequestCustom");
    }

    RequestCustom(GameSpecificPacketHandler &handler, sf::Packet &packet) :
        NetworkRequest(NETWORK_REQUEST_TYPE::Custom)
    {
        ActualRequest = handler.ReadGameSpecificPacketFromPacket(false, packet);
        
        if(!ActualRequest){

            throw InvalidArgument("invalid packet format for user defined request");
        }
    }

    inline void AddDataToPacket(GameSpecificPacketHandler &handler, sf::Packet &packet){

        packet << static_cast<uint16_t>(Type);

        handler.PassGameSpecificDataToPacket(ActualRequest.get(), packet);
    }

    std::shared_ptr<GameSpecificPacketData> ActualRequest;
};

//! \brief Empty request for ones that require no data
//!
//! Also used for all other request that don't need any data members
class RequestEcho : public NetworkRequest {
public:
    RequestEcho(NETWORK_REQUEST_TYPE actualtype) :
        NetworkRequest(actualtype)
    {}

    void _SerializeCustom(sf::Packet &packet) override{
    }

    RequestEcho(NETWORK_REQUEST_TYPE actualtype, uint32_t idforresponse, sf::Packet &packet) :
        NetworkRequest(actualtype, idforresponse)
    {
    }
};



// This file is generated by the script GenerateRequest.rb
// and contains implementations for all the response types
#include "../Generated/RequestImpl.h"


}

