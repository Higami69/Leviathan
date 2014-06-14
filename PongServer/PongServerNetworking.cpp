#include "PongIncludes.h"
// ------------------------------------ //
#ifndef PONGSERVERNETWORKING
#include "PongServerNetworking.h"
#endif
#include "Networking/NetworkRequest.h"
#include "Networking/ConnectionInfo.h"
#include "Networking/NetworkResponse.h"
#include "Gameplay/CustomCommandHandler.h"
#include "PongServer.h"
#include "PlayerSlot.h"
using namespace Pong;
// ------------------------------------ //
Pong::PongServerNetworking::PongServerNetworking() : NetworkServerInterface(8, L"Local pong game", Leviathan::NETWORKRESPONSE_SERVERJOINRESTRICT_NONE),
	ServerStatusIs(PONG_JOINGAMERESPONSE_TYPE_LOBBY)
{
	RegisterCustomCommandHandlers(_CommandHandler);
}

Pong::PongServerNetworking::~PongServerNetworking(){

}
// ------------------------------------ //
void Pong::PongServerNetworking::HandleResponseOnlyPacket(shared_ptr<Leviathan::NetworkResponse> message, Leviathan::ConnectionInfo* connection, bool &dontmarkasreceived){
	// Try default handling //
	if(_HandleDefaultResponseOnly(message, connection, dontmarkasreceived))
		return;
	if(_HandleServerResponseOnly(message, connection, dontmarkasreceived))
		return;

	// Check for custom packets //
	if(message->GetType() == Leviathan::NETWORKRESPONSETYPE_CUSTOM){
		// Handle the game specific //
		GameSpecificPacketData* data = message->GetResponseDataForGameSpecific()->ActualPacketData.get();

		// Check is it invalid type //
		if(data->IsRequest)
			return;

		//switch(data->TypeIDNumber){
		//}

	}

	// We couldn't handle it //
	Logger::Get()->Error(L"Couldn't handle a packet");
}

void Pong::PongServerNetworking::HandleRequestPacket(shared_ptr<NetworkRequest> request, ConnectionInfo* connection){
	// Try default handling //
	if(_HandleDefaultRequest(request, connection))
		return;
	// Try server handling //
	if(_HandleServerRequest(request, connection))
		return;

	// Check for custom packets //
	if(request->GetType() == Leviathan::NETWORKREQUESTTYPE_CUSTOM){
		// Handle the game specific //
		GameSpecificPacketData* data = request->GetCustomRequestData()->ActualPacketData.get();

		// Check is it invalid type //
		if(!data->IsRequest)
			return;

		switch(data->TypeIDNumber){
		case PONG_PACKET_JOINGAME_REQUEST:
			{
				// Disallow if not connected //
				GUARD_LOCK_THIS_OBJECT();

				Leviathan::ConnectedPlayer* ply = GetPlayerForConnection(connection);

				if(!ply){

					shared_ptr<NetworkResponse> tmpresponse(new NetworkResponse(request->GetExpectedResponseID(), 
						Leviathan::PACKAGE_TIMEOUT_STYLE_TIMEDMS, 1000));
					// Send the state to the player //
					tmpresponse->GenerateServerDisallowResponse(new Leviathan::NetworkResponseDataForServerDisallow(
						Leviathan::NETWORKRESPONSE_INVALIDREASON_UNAUTHENTICATED, L"Connection not connected as a player"));

					connection->SendPacketToConnection(tmpresponse, 1);
					return;
				}

				// Move the player to the proper state and send that state back to the player //
				// We now require the player to send heartbeat packets //
				ply->StartHeartbeats();

				shared_ptr<NetworkResponse> tmpresponse(new NetworkResponse(request->GetExpectedResponseID(), 
					Leviathan::PACKAGE_TIMEOUT_STYLE_TIMEDMS, 1000));
				// Send the state to the player //
				tmpresponse->GenerateCustomResponse(new PongJoinGameResponse(ServerStatusIs));

				connection->SendPacketToConnection(tmpresponse, 3);
				
				return;
			}
		}
	}


	// We couldn't handle it //
	Logger::Get()->Error(L"Couldn't handle a packet");
}

void Pong::PongServerNetworking::TickIt(){
	// Tick the server //
	UpdateServerStatus();
}
// ------------------------------------ //
void Pong::PongServerNetworking::CloseDown(){
	CloseDownServer();
}
// ------------------------------------ //
void Pong::PongServerNetworking::RegisterCustomCommandHandlers(CommandHandler* addhere){
	addhere->RegisterCustomCommandHandler(make_shared<PongCommandHandler>(this));
}
// ------------------------------------ //
void Pong::PongServerNetworking::_OnPlayerDisconnect(Leviathan::ConnectedPlayer* newplayer){

	auto veccy = PongServer::Get()->GetPlayers()->GetVec();

	auto end = veccy.end();
	for(auto iter = veccy.begin(); iter != end; ++iter){

		if((*iter)->GetConnectedPlayer() == newplayer){

			(*iter)->SlotLeavePlayer();
			PongServer::Get()->GetPlayers()->NotifyUpdatedValue();

			// \todo If the game is in progress update the level to block of the slot
			return;
		}
	}
}





