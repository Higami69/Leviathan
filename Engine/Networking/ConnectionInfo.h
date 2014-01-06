#ifndef LEVIATHAN_CONNECTIONINFO
#define LEVIATHAN_CONNECTIONINFO
// ------------------------------------ //
#ifndef LEVIATHAN_DEFINE
#include "Define.h"
#endif
// ------------------------------------ //
// ---- includes ---- //
#include "NetworkResponse.h"
#include "NetworkRequest.h"
#include "SFML/Network/Socket.hpp"
#include "SFML/Network/UdpSocket.hpp"
#include "SFML/Network/IpAddress.hpp"
#include "Common/ThreadSafe.h"
#include <boost/thread/future.hpp>
#include "NetworkHandler.h"

namespace Leviathan{

#define DEFAULT_ACKCOUNT		32
#define KEEPALIVE_TIME			2000
#define ACKKEEPALIVE			50


// Makes the program spam a ton of debug info about packets //
#define SPAM_ME_SOME_PACKETS	1

	struct SentNetworkThing{

		// This is the signature for request packets //
		SentNetworkThing(int packetid, int expectedresponseid, shared_ptr<NetworkRequest> request, shared_ptr<boost::promise<bool>> waitobject, 
			int maxtries, PACKET_TIMEOUT_STYLE howtotimeout, int timeoutvalue, const sf::Packet &packetsdata, int attempnumber = 1);
		// This is the signature for response packets //
		SentNetworkThing(int packetid, shared_ptr<NetworkResponse> response, shared_ptr<boost::promise<bool>> waitobject, int maxtries, 
			PACKET_TIMEOUT_STYLE howtotimeout, int timeoutvalue, const sf::Packet &packetsdata, int attempnumber = 1);


		int PacketNumber;

		int MaxTries;
		int AttempNumber;

		PACKET_TIMEOUT_STYLE PacketTimeoutStyle;

		int TimeOutMS;
		__int64 RequestStartTime;
		__int64 ConfirmReceiveTime;
		int ExpectedResponseID;
		shared_ptr<boost::promise<bool>> WaitForMe;

		// This is stored for resending the data //
		sf::Packet AlmostCompleteData;

		// If set the following variables will be used //
		bool IsArequest;
		shared_ptr<NetworkResponse> GotResponse;
		shared_ptr<NetworkRequest> OriginalRequest;
		// Else (if not a request) no response is expected (other than a receive confirmation) //
		shared_ptr<NetworkResponse> SentResponse;
	};

	static_assert(sizeof(char) == 1, "Char must be one byte in size");
	static_assert(sizeof(int) == 4, "Int must be four bytes in size");

	typedef std::map<int, bool> ReceivedPacketField;

	class NetworkAckField{
	public:

		DLLEXPORT NetworkAckField(){};
		DLLEXPORT NetworkAckField(sf::Int32 firstpacketid, char maxacks, ReceivedPacketField &copyfrom);

		DLLEXPORT inline bool IsAckSet(size_t ackindex){
			// We can use division to find out which vector element is wanted //
			size_t vecelement = ackindex/8;

			return (Acks[vecelement] & (1 << (ackindex-vecelement))) != 0;
		}

		// If the ack in this field is set then it is set in the argument map, but if ack is not set in this field it isn't reseted in the argument map //
		DLLEXPORT void SetPacketsReceivedIfNotSet(ReceivedPacketField &copydatato);

		DLLEXPORT void RemoveMatchingPacketIDsFromMap(ReceivedPacketField &removefrom);

		// Data //
		sf::Int32 FirstPacketID;
		vector<sf::Int8> Acks;
	};

	struct SentAcks{

		SentAcks(int packet, NetworkAckField* newddata);
		~SentAcks();

		// The packet (SentNetworkThing) in which these acks were sent //
		int InsidePacket;
		//! Used to control how many times to send each ackbunch //
		//! If package loss is high this will be increased to make sure acks are received //
		int SendCount;

		//! Marks if this can be deleted (after using for resends, of course) //
		bool Received;

		NetworkAckField* AcksInThePacket;
	};


	class ConnectionInfo : public ThreadSafe{
	public:
		DLLEXPORT ConnectionInfo(shared_ptr<wstring> hostname);
		DLLEXPORT ConnectionInfo(const sf::IpAddress &targetaddress, USHORT port);
		DLLEXPORT ~ConnectionInfo();

		// Creates the address object //
		DLLEXPORT bool Init();
		DLLEXPORT void Release();

		DLLEXPORT bool IsThisYours(sf::Packet &packet, sf::IpAddress &sender, USHORT &sentport);

		DLLEXPORT void UpdateListening();

		DLLEXPORT shared_ptr<SentNetworkThing> SendPacketToConnection(shared_ptr<NetworkRequest> request, int maxretries);
		DLLEXPORT shared_ptr<SentNetworkThing> SendPacketToConnection(shared_ptr<NetworkResponse> response, int maxtries);

		// Data exchange functions //
		DLLEXPORT shared_ptr<NetworkResponse> SendRequestAndBlockUntilDone(shared_ptr<NetworkRequest> request, int maxtries = 2);

		DLLEXPORT void SendKeepAlivePacket();

	private:

		//void _PopMadeRequest(shared_ptr<SentNetworkThing> objectptr, ObjectLock &guard);
		void _ResendRequest(shared_ptr<SentNetworkThing> toresend, ObjectLock &guard);

		// Marks the acks in packet received as successfully sent and erases them //
		void _VerifyAckPacketsAsSuccesfullyReceivedFromHost(int packetreceived);

		void _PreparePacketHeaderForPacket(int packetid, sf::Packet &tofill, bool isrequest);

		shared_ptr<SentNetworkThing> _GetPossibleRequestForResponse(shared_ptr<NetworkResponse> response);
		// ------------------------------------ //

		// Packet sent and received data //
		std::map<int, bool> SentPacketsConfirmedAsReceived;
		std::map<int, bool> ReceivedPacketsNotifiedAsReceivedByUs;
		int LastSentConfirmID;

		int MyLastSentReceived;

		// Holds the ID of the last sent packet //
		int LastUsedID;

		// How many times the same ack table is sent before new one is generated (usually 1 with good connections) //
		int MaxAckReduntancy;

		__int64 LastSentPacketTime;


		// Sent packets that haven't been confirmed as arrived //
		std::list<shared_ptr<SentNetworkThing>> WaitingRequests;

		std::vector<shared_ptr<SentAcks>> AcksNotConfirmedAsReceived;

		USHORT TargetPortNumber;
		shared_ptr<wstring> HostName;
		sf::IpAddress TargetHost;
		bool AddressGot;
	};

}
#endif