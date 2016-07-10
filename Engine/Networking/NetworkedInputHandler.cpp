// ------------------------------------ //
#include "NetworkedInputHandler.h"

#include "NetworkedInput.h"
#include "NetworkRequest.h"
#include "NetworkResponse.h"
#include "Connection.h"
#include "Threading/ThreadingManager.h"
#include "NetworkServerInterface.h"
#include "../Utility/Convert.h"
#include "Exceptions.h"
using namespace Leviathan;
using namespace std;
// ------------------------------------ //
DLLEXPORT Leviathan::NetworkedInputHandler::NetworkedInputHandler(
    NetworkInputFactory* objectcreater, NetworkClientInterface* isclient) : 
	IsOnTheServer(false), LastInputSourceID(2500),
    _NetworkInputFactory(objectcreater), ClientInterface(isclient)
{
}

DLLEXPORT Leviathan::NetworkedInputHandler::NetworkedInputHandler(
    NetworkInputFactory* objectcreater, NetworkServerInterface* isserver) : 
	IsOnTheServer(true), LastInputSourceID(2500), 
    _NetworkInputFactory(objectcreater), ServerInterface(isserver)
{
}

DLLEXPORT Leviathan::NetworkedInputHandler::~NetworkedInputHandler(){
    
}
// ------------------------------------ //
DLLEXPORT void Leviathan::NetworkedInputHandler::Release(){
    
	GUARD_LOCK();

    _HandleDeleteQueue(guard);

    for(size_t i = 0; i < GlobalOrLocalListeners.size(); i++){

        // Apparently there can be null pointers in the vector, skip them //
        NetworkedInput* ptr = GlobalOrLocalListeners[i].get();

        if(!ptr)
            continue;

        ptr->_UnConnectParent(guard);
		ptr->TerminateConnection();
		_NetworkInputFactory->NoLongerNeeded(*ptr, guard);

        if(ptr->ConnectedTo){

            DEBUG_BREAK;
        }
    }
    
    GlobalOrLocalListeners.clear();

    // The listeners might want to destruct stuff, so set this to NULL after releasing them //
	_NetworkInputFactory = NULL;    
}
// ------------------------------------ //
DLLEXPORT bool Leviathan::NetworkedInputHandler::HandleInputPacket(
    shared_ptr<NetworkRequest> request,
    Connection &connection)
{
	switch(request->GetType()){
    case NETWORK_REQUEST_TYPE::ConnectInput:
		{
            GUARD_LOCK();

			// Clients won't receive these //
			if(!IsOnTheServer) 
				return false;

			_HandleConnectRequestPacket(guard, request, connection);
			return true;
		}
        default:
        {
            // Type didn't match anything that we should be concerned with //
            return false;
        }
	}
}

DLLEXPORT bool Leviathan::NetworkedInputHandler::HandleInputPacket(
    shared_ptr<NetworkResponse> response,
    Connection &connection)
{
	switch(response->GetType()){
    case NETWORK_RESPONSE_TYPE::CreateNetworkedInput:
		{
            GUARD_LOCK();

			// Server in turn ignores this one //
			if(IsOnTheServer) 
				return false;
            
#ifndef NETWORK_USE_SNAPSHOTS
            if(!_HandleInputCreateResponse(guard, response, connection)){

                Logger::Get()->Error("NetworkedInputHandler: failed to create replicated input on a client");
                return true;
            }
#endif //NETWORK_USE_SNAPSHOTS
            
			return true;
		}
    case NETWORK_RESPONSE_TYPE::DisconnectInput:
        {
            GUARD_LOCK();

			// Clients won't receive these //
			if(!IsOnTheServer) 
				return false;

			_HandleDisconnectRequestPacket(guard, response, connection);

            return true;
        }

    case NETWORK_RESPONSE_TYPE::UpdateNetworkedInput:
		{
            GUARD_LOCK();
            
			// Process it //
			if(!_HandleInputUpdateResponse(guard, response, connection)){

				// This packet wasn't properly authenticated //
                Logger::Get()->Warning("NetworkedInputHandler: improperly authenticated response");
				return true;
			}

#ifndef NETWORK_USE_SNAPSHOTS
			// Everybody receives these, but only the server has to distribute these around //
            if(IsOnTheServer){

                // Distribute it around //
        
                // We duplicate the packet from the data to make it harder for people to send invalid packets to
                // other clients 
                std::shared_ptr<NetworkResponse> tmprespall = std::shared_ptr<NetworkResponse>(new NetworkResponse(-1,
                        PACKET_TIMEOUT_STYLE_PACKAGESAFTERRECEIVED, 5));

                tmprespall->GenerateUpdateNetworkedInputResponse(new NetworkResponseDataForUpdateNetworkedInput(
                        *response->GetResponseDataForUpdateNetworkedInputResponse()));

                ServerInterface->SendToAllButOnePlayer(response, connection);
            }
#endif //NETWORK_USE_SNAPSHOTS

			return true;
		}
        default:
        {
            // Type didn't match anything that we should be concerned with //
            return false;
        }
	}
}
// ------------------------------------ //
void Leviathan::NetworkedInputHandler::_HandleConnectRequestPacket(Lock &guard,
    shared_ptr<NetworkRequest> request, Connection &connection)
{
    // Verify that it is allowed //
    bool allowed = true;

    if (request->GetType() != NETWORK_REQUEST_TYPE::ConnectInput)
        goto notallowedfailedlabel;

    // If the packet is valid it should contain this //
    auto* data = static_cast<RequestConnectInput*>(request.get());

    {

        // We need to partially load the data here //
        int ownerid, inputid;

        NetworkedInput::LoadHeaderDataFromPacket(data->DataForObject, ownerid, inputid);

        // Create a temporary object from the packet //
        auto ournewobject = _NetworkInputFactory->CreateNewInstanceForReplication(inputid,
            ownerid);

        if(!ournewobject)
            goto notallowedfailedlabel;

        // Check is it allowed //
        allowed = _NetworkInputFactory->DoesServerAllowCreate(ournewobject.get(), connection);


        if(!allowed)
            goto notallowedfailedlabel;


        // It got accepted so finish adding the data //
        ournewobject->OnLoadCustomFullDataFrompacket(data->DataForObject);

        // Set status //
        ournewobject->SetAsServerSize();

        // Add it to us //
        LinkReceiver(guard, ournewobject.get());
        ournewobject->NowOwnedBy(this);
        ournewobject->SetNetworkReceivedState();


        GlobalOrLocalListeners.push_back(shared_ptr<NetworkedInput>(ournewobject.release()));

        _NetworkInputFactory->ReplicationFinalized(GlobalOrLocalListeners.back().get());

        // Notify that we accepted it //
        ResponseServerAllow response(request->GetIDForResponse(),
            SERVER_ACCEPTED_TYPE::ConnectAccepted);

        connection.SendPacketToConnection(response, RECEIVE_GUARANTEE::Critical);

        // Send messages to other clients //
        
#ifndef NETWORK_USE_SNAPSHOTS
        // TODO: remove this block of code
        // First create the packet //
        shared_ptr<NetworkResponse> tmprespall = std::shared_ptr<NetworkResponse>(
            new NetworkResponse(-1, PACKET_TIMEOUT_STYLE_PACKAGESAFTERRECEIVED, 15));

        tmprespall->GenerateCreateNetworkedInputResponse(
            new NetworkResponseDataForCreateNetworkedInput(*GlobalOrLocalListeners.back()));

        // Using threading here to not use too much time processing the create request //

        // \todo Guarantee that the interface will be available when this is ran
        ThreadingManager::Get()->QueueTask(new QueuedTask(boost::bind<void>(
                    [](shared_ptr<NetworkResponse> response, NetworkServerInterface* server,
                        Connection* skipme) -> void
            {
                
                // Then tell the interface to send it to all but one connection //
                server->SendToAllButOnePlayer(response, skipme);

                Logger::Get()->Info("NetworkedInputHandler: finished distributing create "
                    "response around");

            }, tmprespall, ServerInterface, connection)));
#endif //NETWORK_USE_SNAPSHOTS



        return;
    }

notallowedfailedlabel:

	// Notify about we disallowing this connection //
    ResponseServerDisallow tmpresp(request->GetIDForResponse(),
        "Not allowed to create input with that ID",
        NETWORK_RESPONSE_INVALIDREASON::NotAuthorized);

	connection.SendPacketToConnection(tmpresp, RECEIVE_GUARANTEE::Critical);
}
// ------------------------------------ //
bool NetworkedInputHandler::_HandleDisconnectRequestPacket(Lock &guard,
    std::shared_ptr<NetworkResponse> response, Connection &connection)
{
#ifndef NETWORK_USE_SNAPSHOTS
#error implement this
#endif

    if (response->GetType() != NETWORK_RESPONSE_TYPE::DisconnectInput)
        return false;

    auto* data = static_cast<ResponseDisconnectInput*>(response.get());

    LOG_WRITE("TODO: security check for _HandleDisconnectRequestPacket");

    for(size_t i = 0; i < GlobalOrLocalListeners.size(); i++){

        auto current = GlobalOrLocalListeners[i].get();

        if(current->InputID == data->InputID && current->OwnerID == data->OwnerID){

            current->_UnConnectParent(guard);
            _NetworkInputFactory->NoLongerNeeded(*current, guard);
            
            GlobalOrLocalListeners.erase(GlobalOrLocalListeners.begin()+i);
            return true;
        }
    }

    return false;
}

// ------------------------------------ //
DLLEXPORT void Leviathan::NetworkedInputHandler::UpdateInputStatus(){
	GUARD_LOCK();

	// Remove invalid things //
	auto end = GlobalOrLocalListeners.end();
	for(auto iter = GlobalOrLocalListeners.begin(); iter != end; ++iter){

		// This checks for all invalid states //
		auto tmpstate = (*iter)->GetState();
		if(tmpstate == NETWORKEDINPUT_STATE_CLOSED || tmpstate == NETWORKEDINPUT_STATE_FAILED){

			DeleteQueue.push_back(*iter);
			iter = GlobalOrLocalListeners.erase(iter);
		}
	}


	_HandleDeleteQueue(guard);
}
// ------------------------------------ //
DLLEXPORT void Leviathan::NetworkedInputHandler::GetNextInputIDNumber(
    std::function<void(int)> onsuccess,
    std::function<void()> onfailure)
{
	GUARD_LOCK();

	DEBUG_BREAK;
}

DLLEXPORT int Leviathan::NetworkedInputHandler::GetNextInputIDNumberOnServer(){

	GUARD_LOCK();

	LEVIATHAN_ASSERT(IsOnTheServer, "cannot call this function on the client");

	return ++LastInputSourceID;
}
// ------------------------------------ //
DLLEXPORT bool Leviathan::NetworkedInputHandler::RegisterNewLocalGlobalReflectingInputSource(
    shared_ptr<NetworkedInput> iobject)
{

	assert(IsOnTheServer != true && "cannot call this function on the server");

	GUARD_LOCK();


	// Start connection to the server //
	iobject->NowOwnedBy(this);

	iobject->InitializeLocal();

	if(!iobject->ConnectToServersideInput()){

		Logger::Get()->Error("NetworkedInputHandler: register local input source failed "
            "because connecting it to the server didn't start properly");
		return false;
	}

	// Store our local copy //
	GlobalOrLocalListeners.push_back(iobject);

	LinkReceiver(guard, iobject.get());

	return true;
}
// ------------------------------------ //
DLLEXPORT void Leviathan::NetworkedInputHandler::QueueDeleteInput(NetworkedInput* inputobj){
	GUARD_LOCK();

	// Find and remove from the main list //
	auto end = GlobalOrLocalListeners.end();
	for(auto iter = GlobalOrLocalListeners.begin(); iter != end; ++iter){

		if((*iter).get() == inputobj){

			// Found the one //
			DeleteQueue.push_back(*iter);
			GlobalOrLocalListeners.erase(iter);
			return;
		}
	}

	// Not found //
}

void Leviathan::NetworkedInputHandler::_HandleDeleteQueue(Lock &guard){

    if(DeleteQueue.empty())
        return;

    Logger::Get()->Info("NetworkedInputHandler: deleting queued input objects");

	for(size_t i = 0; i < DeleteQueue.size(); i++){

        auto current = DeleteQueue[i].get();

        current->_UnConnectParent(guard);
		current->TerminateConnection();
		_NetworkInputFactory->NoLongerNeeded(*current, guard);
	}

	// All are now ready to be deleted //
	DeleteQueue.clear();
}
// ------------------------------------ //
bool Leviathan::NetworkedInputHandler::_HandleInputUpdateResponse(Lock &guard,
    shared_ptr<NetworkResponse> response, Connection* connection)
{


	NetworkResponseDataForUpdateNetworkedInput* data = response->GetResponseDataForUpdateNetworkedInputResponse();

    if(!data)
        return false;

	// Find the right input object //

	NetworkedInput* target = NULL;

	auto end = GlobalOrLocalListeners.end();
	for(auto iter = GlobalOrLocalListeners.begin(); iter != end; ++iter){

		// Find the right one //
		if((*iter)->GetID() == data->InputID){

			// Found the target //
			target = (*iter).get();
			break;
		}
	}


	// If we didn't find it this *should* be a bogus request //
	if(!target){
        
        Logger::Get()->Warning("NetworkedInputHandler: couldn't find a target for update response, InputID: "+
            Convert::ToString(data->InputID));
		return false;
    }


	// Check is it allowed //
    if(IsOnTheServer){
        
        if(!_NetworkInputFactory->IsConnectionAllowedToUpdate(target, connection)){

            // Some player is trying to fake someone else's input //
            Logger::Get()->Warning("NetworkedInputHandler: connection not allowed to update InputID: "+
                Convert::ToString(data->InputID));
            return false;
        }
        
    } else {

        if(!connection)
            return false;
    }

	
	// Now we can update it //
	target->LoadUpdatesFromPacket(data->UpdateData);

	return true;
}
// ------------------------------------ //
bool Leviathan::NetworkedInputHandler::_HandleInputCreateResponse(Lock &guard,
    shared_ptr<NetworkResponse> response, Connection* connection)
{
    NetworkResponseDataForCreateNetworkedInput* data = response->GetResponseDataForCreateNetworkedInputResponse();

    if(!data)
        return false;


    // We need to partially load the data here //
    int ownerid, inputid;

    NetworkedInput::LoadHeaderDataFromPacket(data->DataForObject, ownerid, inputid);

    Logger::Get()->Info("NetworkedInputHandler: client replicating networked input, "+Convert::ToString(ownerid));

    // Create a temporary object from the packet //
    auto ournewobject = _NetworkInputFactory->CreateNewInstanceForReplication(inputid, ownerid);

    if(!ournewobject)
        return false;

    // Check is it allowed //
    // Check here is the connection the connection to the server //
    bool allowed = connection ? true: false;

    if(!allowed)
        return false;

    assert(!IsOnTheServer && "Don't call _HandleInputCreateResponse on the server");
    
    // It got accepted so finish adding the data //
    ournewobject->OnLoadCustomFullDataFrompacket(data->DataForObject);
    
    // Add it to us //
    LinkReceiver(guard, ournewobject.get());
    ournewobject->NowOwnedBy(this);
    ournewobject->SetNetworkReceivedState();


    GlobalOrLocalListeners.push_back(shared_ptr<NetworkedInput>(ournewobject.release()));

    _NetworkInputFactory->ReplicationFinalized(GlobalOrLocalListeners.back().get());

	return true;
}
// ------------------------------------ //
std::unique_ptr<NetworkedInput> NetworkInputFactory::CreateNewInstanceForLocalStart(
    int inputid, bool isclient)
{
    // This should not be called, the child class should hide this or not call this variant
    DEBUG_BREAK;
    throw Exception("Don't call this function");
}


