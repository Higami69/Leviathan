// ------------------------------------ //
#include "NetworkServerInterface.h"

#include "NetworkRequest.h"
#include "Connection.h"
#include "Gameplay/CommandHandler.h"
#include "SyncedVariables.h"
#include "Networking/NetworkCache.h"
#include "Entities/GameWorld.h"
#include "../TimeIncludes.h"
#include "NetworkInterface.h"
#include "NetworkHandler.h"
using namespace Leviathan;
// ------------------------------------ //
DLLEXPORT NetworkServerInterface::NetworkServerInterface(
    int maxplayers, const std::string &servername, 
    SERVER_JOIN_RESTRICT restricttype, int additionalflags /*= 0*/) :
    NetworkInterface(NETWORKED_TYPE::Server),
    MaxPlayers(maxplayers), ServerName(servername), JoinRestrict(restricttype),
    ExtraServerFlags(additionalflags),
    _CommandHandler(new CommandHandler(this))
{

}

DLLEXPORT NetworkServerInterface::~NetworkServerInterface(){

    // Release the memory //
    for(auto iter = ServerPlayers.begin(); iter != ServerPlayers.end(); ){

        // We can now report errors here as this needs to be deleted
        // before engine is released
        LOG_WARNING("Warning! server quitting while player list has data in it");
        
        iter = ServerPlayers.erase(iter);
    }
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::HandleRequestPacket(
    std::shared_ptr<NetworkRequest> request, Connection &connection) 
{
    LEVIATHAN_ASSERT(request, "request is null");

    if(_HandleDefaultRequest(request, connection))
        return;

    switch (request->GetType()) {
    case NETWORK_REQUEST_TYPE::RequestCommandExecution:
    {
        // Get the matching player //
        auto ply = GetPlayerForConnection(connection);

        // Drop it if no matching players //
        if(!ply)
            return;

        // Send a response to the sender //
        connection.SendPacketToConnection(std::make_shared<ResponseNone>(
                NETWORK_RESPONSE_TYPE::None), RECEIVE_GUARANTEE::Critical);

        // Extract the command //
        auto* data = static_cast<RequestRequestCommandExecution*>(request.get());

        // Execute it //
        _CommandHandler->QueueCommand(data->Command, ply.get());

        return;
    }
    case NETWORK_REQUEST_TYPE::Serverstatus:
    {
        RespondToServerStatusRequest(request, connection);
        return;
    }
    case NETWORK_REQUEST_TYPE::JoinServer:
    {
        // Call handling function //
        Logger::Get()->Info("NetworkServerInterface: player on " + connection.
            GenerateFormatedAddressString() + "is trying to connect");

        _HandleServerJoinRequest(request, connection);
        return;
    }
    default:
        break;
    }

    if(_CustomHandleRequestPacket(request, connection))
        return;

    LOG_ERROR("NetworkServerInterface: failed to handle request of type: " +
        Convert::ToString(static_cast<int>(request->GetType())));
}

DLLEXPORT void NetworkServerInterface::HandleResponseOnlyPacket(
    std::shared_ptr<NetworkResponse> message, Connection &connection) 
{
    LEVIATHAN_ASSERT(message, "message is null");

    if(_HandleDefaultResponseOnly(message, connection))
        return;

    switch (message->GetType()) {
    case NETWORK_RESPONSE_TYPE::ServerHeartbeat:
    {
        // Notify the matching player object about a heartbeat //
        auto ply = GetPlayerForConnection(connection);

        if(!ply) {

            Logger::Get()->Warning("NetworkServerInterface: received a heartbeat packet "
                "from a non-existing player");
            return;
        }

        ply->HeartbeatReceived();

        // Avoid spamming packets back //
        //dontmarkasreceived = true;

        return;
    }
    default:
        break;
    }

    if(_CustomHandleResponseOnlyPacket(message, connection))
        return;

    LOG_ERROR("NetworkServerInterface: failed to handle response of type: " +
        Convert::ToString(static_cast<int>(message->GetType())));
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::CloseDown(){

    _OnCloseDown();

    // Prevent new players //
    ServerStatus = SERVER_STATUS::Shutdown;
    AllowJoin = false;

    for(auto iter = ServerPlayers.begin(); iter != ServerPlayers.end(); ){

        // Kick them //
        (*iter)->OnKicked("Server closing");

        iter = ServerPlayers.erase(iter);
    }

    _CommandHandler.reset();
}
// ------------------------------------ //
DLLEXPORT std::shared_ptr<ConnectedPlayer> NetworkServerInterface::GetPlayerForConnection(
    Connection &connection)
{
    // Search through the connections //
    
    for(size_t i = 0; i < ServerPlayers.size(); i++){
        // Check with the pointer //
        if(ServerPlayers[i]->IsConnectionYours(&connection))
            return ServerPlayers[i];
    }

    return nullptr;
}

DLLEXPORT std::vector<std::shared_ptr<Leviathan::Connection>>& 
NetworkServerInterface::GetClientConnections() 
{
    return ServerPlayersConnections;
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::RespondToServerStatusRequest(
    std::shared_ptr<NetworkRequest> request, Connection &connectiontouse)
{
    // Gather info for a response //
    ResponseServerStatus response(request->GetIDForResponse(), ServerName,
        AllowJoin, JoinRestrict, ServerStatus, static_cast<int32_t>(ServerPlayers.size()), 
        MaxPlayers, 
        static_cast<int32_t>(ActiveBots.size()), ExtraServerFlags);

    // Send it //
    connectiontouse.SendPacketToConnection(response);
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::SetServerStatus(SERVER_STATUS newstatus){
    ServerStatus = newstatus;
}

DLLEXPORT void NetworkServerInterface::SetServerAllowPlayers(bool allowingplayers){
    AllowJoin = allowingplayers;
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::_HandleServerJoinRequest(
    std::shared_ptr<NetworkRequest> request,
    Connection &connection)
{
    if(!AllowJoin){

        ResponseServerDisallow response(request->GetIDForResponse(), 
            "Server is not accepting any players at this time",
            NETWORK_RESPONSE_INVALIDREASON::ServerNotAcceptingPlayers);

        connection.SendPacketToConnection(response);
        return;
    }

    // Check is the player already connected //
    if(GetPlayerForConnection(connection)) {

        ResponseServerDisallow response(request->GetIDForResponse(),
            "You are already connected to this server, disconnect first",
            NETWORK_RESPONSE_INVALIDREASON::ServerAlreadyConnectedToYou);

        connection.SendPacketToConnection(response);
        return;
    }

    // Call this here, so this can potentially kick players for reserved slots //
    PlayerPreconnect(connection, request);

    // Check if we can fit a new player //
    if((int)(ServerPlayers.size()+1) > MaxPlayers){

        std::string plys = Convert::ToString(ServerPlayers.size());

        ResponseServerDisallow response(request->GetIDForResponse(),
            "Server is at maximum capacity, " + plys + "/" + plys,
            NETWORK_RESPONSE_INVALIDREASON::ServerFull);

        connection.SendPacketToConnection(response);
        return;
    }

    // Connection security check //
    if(connection.GetState() != CONNECTION_STATE::Authenticated) {

        ResponseServerDisallow response(request->GetIDForResponse(),
            "Connection state is invalid",
            NETWORK_RESPONSE_INVALIDREASON::Unauthenticated);

        connection.SendPacketToConnection(response);
        return;
    }

    // Do something with join restrict things //


    // Check if the program wants to veto this join //
    std::string disallowmessage;

    if(!AllowPlayerConnectVeto(request, connection, disallowmessage)){

        ResponseServerDisallow response(request->GetIDForResponse(),
            disallowmessage,
            NETWORK_RESPONSE_INVALIDREASON::ServerCustom);

        connection.SendPacketToConnection(response);
        return;
    }

    // Player joined! //
    int newid = ++CurrentPlayerID;
    
    ServerPlayers.push_back(std::make_shared<ConnectedPlayer>(
            Owner->GetConnection(&connection), this, newid));
        
    _OnReportPlayerConnected(ServerPlayers.back(), connection);
    
    Logger::Get()->Info("NetworkServerInterface: accepted a new player, ID: "+
        Convert::ToString(newid));

    // Send connection notification back to the client //
    ResponseServerAllow response(request->GetIDForResponse(),
        SERVER_ACCEPTED_TYPE::ConnectAccepted, "Allowed, ID: " +
        Convert::ToString(newid));

    connection.SendPacketToConnection(response);
}
// ------------------ Default callbacks ------------------ //
DLLEXPORT void NetworkServerInterface::_OnPlayerConnected(
    std::shared_ptr<ConnectedPlayer> newplayer)
{

}

DLLEXPORT void NetworkServerInterface::_OnPlayerDisconnect(
    std::shared_ptr<ConnectedPlayer> newplayer)
{

}

DLLEXPORT bool NetworkServerInterface::PlayerPotentiallyKicked(ConnectedPlayer* player){
    return true;
}

DLLEXPORT bool NetworkServerInterface::AllowPlayerConnectVeto(
    std::shared_ptr<NetworkRequest> request, Connection &connection, std::string &message)
{
    return true;
}

DLLEXPORT void NetworkServerInterface::PlayerPreconnect(Connection &connection,
    std::shared_ptr<NetworkRequest> joinrequest)
{

}

DLLEXPORT void NetworkServerInterface::RegisterCustomCommandHandlers(
    CommandHandler* addhere)
{

}
// ------------------------------------ //
void NetworkServerInterface::_OnReportCloseConnection(std::shared_ptr<ConnectedPlayer> plyptr){
    // Close common interfaces that might be using this player //

    // Make sure player's connection is removed from client connections
    for (auto iter = ServerPlayersConnections.begin(); iter != ServerPlayersConnections.end();
        ++iter)
    {
        if((*iter) == plyptr->GetConnection()) {

            ServerPlayersConnections.erase(iter);
            break;
        }
    }

    Logger::Get()->Info("NetworkServerInterface: player \"" + plyptr->GetNickname() +
        "\" unconnected");

    _OnPlayerDisconnect(plyptr);
}

void NetworkServerInterface::_OnReportPlayerConnected(
    std::shared_ptr<ConnectedPlayer> plyptr, Connection &connection)
{
    Logger::Get()->Info("NetworkServerInterface: player \"" + plyptr->GetNickname() +
        "\" connected");

    // Make sure player's connection is in client connections
    ServerPlayersConnections.push_back((plyptr->GetConnection()));

    _OnPlayerConnected(plyptr);
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::TickIt(){
        
    // Check for closed connections //
    auto end = ServerPlayers.end();
    for(auto iter = ServerPlayers.begin(); iter != end; ){
            
        if((*iter)->IsConnectionClosed()){

            // The player has disconnected //
            _OnReportCloseConnection(*iter);

            iter = ServerPlayers.erase(iter);
            // The end iterator is now also invalid //
            end = ServerPlayers.end();
            continue;
        }

        (*iter)->UpdateHeartbeats();
        ++iter;
    }
    

    // Update the command handling //
    _CommandHandler->UpdateStatus();
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::SendToAllButOnePlayer(
    const std::shared_ptr<NetworkResponse> &response,
    Connection* skipme, RECEIVE_GUARANTEE guarantee)
{
 // Loop the players and send to their connections //
    for(auto iter = ServerPlayers.begin(); iter != ServerPlayers.end(); ++iter){

        Connection* curconnection = (*iter)->GetConnection().get();

        if(curconnection != skipme){

            curconnection->SendPacketToConnection(response, guarantee);
        }
    }
}

DLLEXPORT void NetworkServerInterface::SendToAllPlayers(
    const std::shared_ptr<NetworkResponse> &response, RECEIVE_GUARANTEE guarantee)
{
    // Loop the players and send to their connections //
    for(auto iter = ServerPlayers.begin(); iter != ServerPlayers.end(); ++iter){

        (*iter)->GetConnection()->SendPacketToConnection(response, guarantee);
    }
}
// ------------------------------------ //
DLLEXPORT void NetworkServerInterface::VerifyWorldIsSyncedWithPlayers(
    std::shared_ptr<GameWorld> world)
{
    // We can safely add all players as they will only be added if they aren't there already //
    for (auto iter = ServerPlayers.begin(); iter != ServerPlayers.end(); ++iter)
        world->SetPlayerReceiveWorld(*iter);
}
// ------------------------------------ //

