#include "Include.h"
// ------------------------------------ //
#ifndef LEVIATHAN_REMOTECONSOLE
#include "RemoteConsole.h"
#endif
#include "ConnectionInfo.h"
#include "Application\Application.h"
using namespace Leviathan;
// ------------------------------------ //
DLLEXPORT Leviathan::RemoteConsole::RemoteConsole() : CloseIfNoRemoteConsole(false){
	staticinstance = this;
}

DLLEXPORT Leviathan::RemoteConsole::~RemoteConsole(){
	staticinstance = NULL;
}

DLLEXPORT RemoteConsole* Leviathan::RemoteConsole::Get(){
	return staticinstance;
}

RemoteConsole* Leviathan::RemoteConsole::staticinstance = NULL;
// ------------------------------------ //
DLLEXPORT void Leviathan::RemoteConsole::UpdateStatus(){
	ObjectLock guard(*this);
	// Check awaiting connections //
	auto timenow = boost::chrono::steady_clock::now();

	for(size_t i = 0; i < AwaitingConnections.size(); i++){
		if(AwaitingConnections[i]->TimeoutTime < timenow){
			// Time it out //
			Logger::Get()->Warning(L"RemoteConsole: Remote console wait connection timed out, token "+Convert::ToWstring(AwaitingConnections[i]->SessionToken));
			AwaitingConnections.erase(AwaitingConnections.begin()+i);
			i--;
			continue;
		}
	}

	// Special checks //
	if(CloseIfNoRemoteConsole){
		// Send close to application if no connection (or waiting for one) //
		if(AwaitingConnections.size() == 0 && RemoteConsoleConnections.size() == 0){
			// Time to close //

			Logger::Get()->Info(L"RemoteConsole: closing the program because CloseIfNoRemoteConsole, and no active connections");
			LeviathanApplication::GetApp()->StartRelease();
		}
	}

}
// ------------------------------------ //
DLLEXPORT bool Leviathan::RemoteConsole::IsAwaitingConnections(){
	return AwaitingConnections.size() != 0;
}

DLLEXPORT void Leviathan::RemoteConsole::ExpectNewConnection(int SessionToken, const wstring &assignname /*= L""*/, bool onlylocalhost /*= false*/, 
	const MillisecondDuration &timeout /*= boost::chrono::seconds(30)*/)
{
	ObjectLock guard(*this);

	AwaitingConnections.push_back(shared_ptr<RemoteConsoleExpect>(new RemoteConsoleExpect(assignname, SessionToken, onlylocalhost, timeout)));
}
// ------------------------------------ //
DLLEXPORT bool Leviathan::RemoteConsole::CanOpenNewConnection(ConnectionInfo* connection, shared_ptr<NetworkRequest> request){
	// Get data from the packet //
	bool local = connection->IsTargetHostLocalhost();

	// Get the token from the packet //
	auto tmpdata = request->GetRemoteConsoleOpenToDataIfPossible();

	auto opennew = request->GetRemoteConsoleAccessRequestDataIfPossible();

	if(!tmpdata && !opennew){
		// Invalid package data/type //
		return false;
	}

	int sessiontoken = tmpdata ? tmpdata->SessionToken: opennew->SessionToken;

	// Look for a matching awaiting connection //
	for(size_t i = 0; i < AwaitingConnections.size(); i++){
		if(AwaitingConnections[i]->OnlyLocalhost && local || !AwaitingConnections[i]->OnlyLocalhost){
			if(AwaitingConnections[i]->SessionToken == sessiontoken){
				// Match found //
				Logger::Get()->Info(L"RemoteConsole: matching connection request got!");

				// Add to real connections //
				RemoteConsoleConnections.push_back(shared_ptr<RemoteConsoleSession>(new RemoteConsoleSession(AwaitingConnections[i]->ConnectionName,
					connection, AwaitingConnections[i]->SessionToken)));

				if(tmpdata){
					// Create a open request //
					shared_ptr<NetworkRequest> tmprequest(new NetworkRequest(new RemoteConsoleAccessRequestData(AwaitingConnections[i]->SessionToken)));

					// Send initial packet //
					connection->SendPacketToConnection(tmprequest, 3);
					WaitingRequests.push_back(tmprequest);

					// We should also reply with an empty response //
					shared_ptr<NetworkResponse> tmpresponse(new NetworkResponse(request->GetExpectedResponseID(), PACKAGE_TIMEOUT_STYLE_TIMEDMS, 5000));
					tmpresponse->GenerateEmptyResponse();

					connection->SendPacketToConnection(tmpresponse, 5);

				} else if(opennew){
					// Open new, send succeed packet back //
					shared_ptr<NetworkResponse> tmpresponse(new NetworkResponse(request->GetExpectedResponseID(), PACKAGE_TIMEOUT_STYLE_TIMEDMS, 1000));
					tmpresponse->GenerateRemoteConsoleOpenedResponse();

					connection->SendPacketToConnection(tmpresponse, 5);
				}

				AwaitingConnections.erase(AwaitingConnections.begin()+i);
				return true;
			}
		}
	}

	// Didn't find a match //
	return false;
}
// ------------------------------------ //
DLLEXPORT void Leviathan::RemoteConsole::OfferConnectionTo(ConnectionInfo* connectiontouse, const wstring &connectionname, int token){
	ObjectLock guard(*this);
	// Add to the expected connections //
	AwaitingConnections.push_back(shared_ptr<RemoteConsoleExpect>(new RemoteConsoleExpect(connectionname, token, 
		connectiontouse->IsTargetHostLocalhost(), boost::chrono::seconds(15))));

	// Send a request that the target connects to us //
	shared_ptr<NetworkRequest> tmprequest(new NetworkRequest(new RemoteConsoleOpenRequestDataTo(token)));


	connectiontouse->SendPacketToConnection(tmprequest, 4);
}
// ------------------------------------ //
void Leviathan::RemoteConsole::_OnNotifierDisconnected(BaseNotifier* parenttoremove){
	// Close the corresponding console session //
	ConnectionInfo* tmpmatch = static_cast<ConnectionInfo*>(parenttoremove);

	ObjectLock guard(*this);

	for(size_t i = 0; i < RemoteConsoleConnections.size(); i++){
		if(RemoteConsoleConnections[i]->GetConnection() == tmpmatch){
			// Close it //
			RemoteConsoleConnections.erase(RemoteConsoleConnections.begin()+i);
			return;
		}
	}
}
// ------------------------------------ //
DLLEXPORT void Leviathan::RemoteConsole::HandleRemoteConsoleRequestPacket(shared_ptr<NetworkRequest> request, ConnectionInfo* connection){
	// First check if it should be handled by CanOpenNewConnection which handless all open connection packets //
	if(request->GetType() == NETWORKREQUESTTYPE_ACCESSREMOTECONSOLE || request->GetType() == NETWORKREQUESTTYPE_OPENREMOTECONSOLETO){

		CanOpenNewConnection(connection, request);
		return;
	}

	// Handle normal RemoteConsole request //
	DEBUG_BREAK;
}

DLLEXPORT void Leviathan::RemoteConsole::HandleRemoteConsoleResponse(shared_ptr<NetworkResponse> response, ConnectionInfo* connection, 
	shared_ptr<NetworkRequest> potentialrequest)
{
	DEBUG_BREAK;
}
// ------------------------------------ //
DLLEXPORT bool Leviathan::RemoteConsole::SendCustomMessage(int entitycustommessagetype, void* dataptr){
	throw std::exception();
}
// ------------------------------------ //
DLLEXPORT void Leviathan::RemoteConsole::SetCloseIfNoRemoteConsole(bool state){
	CloseIfNoRemoteConsole = state;
}
// ------------------ RemoteConsoleExpect ------------------ //
Leviathan::RemoteConsole::RemoteConsoleExpect::RemoteConsoleExpect(const wstring &name, int token, bool onlylocalhost, const MillisecondDuration 
	&timeout) : ConnectionName(name), SessionToken(token), OnlyLocalhost(onlylocalhost), TimeoutTime(boost::chrono::steady_clock::now()+timeout)
{

}
// ------------------ RemoteConsoleSession ------------------ //
Leviathan::RemoteConsoleSession::RemoteConsoleSession(const wstring &name, ConnectionInfo* connection, int token) : ConnectionName(name), 
	SessionToken(token), CorrespondingConnection(connection)
{

}

Leviathan::RemoteConsoleSession::~RemoteConsoleSession(){
	// Send close request //
	DEBUG_BREAK;

}
// ------------------------------------ //
DLLEXPORT ConnectionInfo* Leviathan::RemoteConsoleSession::GetConnection(){
	return CorrespondingConnection;
}