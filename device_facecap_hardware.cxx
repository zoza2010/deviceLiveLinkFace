
/**	\file	device_facecap_hardware.cxx
*	Developed by Sergei <Neill3d> Solokhin 2019
*	e-mail to: s@neill3d.com
*	twitter: @Neill3d
*
* OpenMoBu github - https://github.com/Neill3d/OpenMoBu
*/


//--- Class declaration
#include "device_facecap_hardware.h"
#include <math.h>
#include <winsock2.h>

///////////////////////////////////////////////////////////////////////


template <typename T>
void unpackByteArray(T& value, const char byteArrayStart[], bool bigEndian = false) {
	if (bigEndian) {
		for (int i = 0; i < sizeof(T); i++)
		{
			reinterpret_cast<char*>(&value)[sizeof(T) - 1 - i] = byteArrayStart[i];
		}
	}
	else {
		for (int i = sizeof(T) - 1; i >= 0; i--)
		{
			reinterpret_cast<char*>(&value)[i] = byteArrayStart[i];
		}
	}
}



bool Cleanup()
{
	if (WSACleanup())
	{
		//         GetErrorStr();
		WSACleanup();

		return false;
	}
	return true;
}
bool Initialize()
{
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		//          GetErrorStr();
		Cleanup();
		return false;
	}
	return true;
}

void bzero(char *b, size_t length)
{
	memset(b, 0, length);
}

int CDevice_FaceCap_Hardware::StartServer(const int server_port)
{
	int lSocket;
	struct protoent* lP;
	struct sockaddr_in  lSin;

	lP = getprotobyname("tcp");

	lSocket = socket(AF_INET, SOCK_DGRAM, 0); // IPPROTO_UDP /*lP->p_proto*/);

	DWORD nonBlocking = 1;
	if (ioctlsocket(lSocket, FIONBIO, &nonBlocking) != 0)
	{
		if (m_Verbose)
		{
			printf("failed to set non-blocking socket\n");
		}
		closesocket(lSocket);
		return 0;
	}

	if (lSocket)
	{
		bzero((char *)&lSin, sizeof(lSin));

		lSin.sin_family = AF_INET;
		lSin.sin_port = htons(server_port);
		lSin.sin_addr.s_addr = INADDR_ANY;

		//Bind socket
		if (bind(lSocket, (struct sockaddr*)&lSin, sizeof(lSin)) < 0)
		{
			if (m_Verbose)
			{
				printf("failed to bind a socket\n");
			}
			closesocket(lSocket);
			return 0;
		}
	}

	return lSocket;
}

/************************************************
 *	Constructor.
 ************************************************/
CDevice_FaceCap_Hardware::CDevice_FaceCap_Hardware()
{
	mParent				= NULL;

	m_Verbose = false;
	
	mNetworkPort		= 11111;
	mStreaming			= true;
	
	mPosition[0]		= 0;
	mPosition[1]		= 0;
	mPosition[2]		= 0;

	mRotation[0]		= 0;
	mRotation[1]		= 0;
	mRotation[2]		= 0;

	Initialize();
}

/************************************************
 *	Destructor.
 ************************************************/
CDevice_FaceCap_Hardware::~CDevice_FaceCap_Hardware()
{
	Cleanup();
}

/************************************************
 *	Set the parent.
 ************************************************/
void CDevice_FaceCap_Hardware::SetParent(FBDevice* pParent)
{
	mParent = pParent;
}

/************************************************
 *	Open device communications.
 ************************************************/
bool CDevice_FaceCap_Hardware::Open()
{
	return true;
}


/************************************************
 *	Close device communications.
 ************************************************/
bool CDevice_FaceCap_Hardware::Close()
{
	StopStream();
	return true;
}


/************************************************
 *	Poll device.
 ************************************************/
bool CDevice_FaceCap_Hardware::PollData()
{
	return true;
}


/************************************************
 *	Fetch a data packet from the device.
 ************************************************/

bool CDevice_FaceCap_Hardware::ProcessMessage(char byteArray[], const int &arraySize)
{
	if (arraySize < 307) {
		/*
		Bytearray consists of permanent bytes sequence + variable part (iphone's name)
		Permanent part contains 306 bytes. We assume that iphone's name contains at least one character.
		Thats why we expect at least 306 + 1 bytes
		*/
		if (m_Verbose) {
			FBTrace("Invalid bytearray length!!! Should contain at least 307 bytes. Ignoring this bytestream.");
		}
		return false;
	}

	int nameLength;
	unpackByteArray(nameLength, byteArray + 41, true);
	int nameEndPos = 45 + nameLength;
	int dataLength = byteArray[nameEndPos + 16];

	if (dataLength != 61) {
		if (m_Verbose) {
			FBTrace("Wrong data block length. should be 61");
		}
		return false;
	}

	char *pCurrentData = byteArray + nameEndPos + 17;

	// blend shapes
	float currentDataValue;
	for (int i = 0; i < static_cast<uint32_t>(EHardwareBlendshapes::count); i++)
	{
		unpackByteArray(currentDataValue, pCurrentData, true);
		m_BlendShapes[facecapToLiveLinkFaceIndexMapping[i]] = static_cast<double>(currentDataValue);
		pCurrentData += sizeof(float);
	}


	// head rotation
	for (int i = 0; i < 3; i++)
	{
		unpackByteArray(currentDataValue, pCurrentData, true);
		mRotation[i] = static_cast<double>(currentDataValue);
		pCurrentData += sizeof(float);
	}

	// left eye rotation
	for (int i = 0; i < 2; i++)
	{
		unpackByteArray(currentDataValue, pCurrentData, true);
		m_LeftEye[i] = static_cast<double>(currentDataValue);
		pCurrentData += sizeof(float);
	}
	pCurrentData += sizeof(float); // Left eye roll is not used thats why we ignore it.

	// right eye rotation
	for (int i = 0; i < 2; i++)
	{
		unpackByteArray(currentDataValue, pCurrentData, true);
		m_RightEye[i] = static_cast<double>(currentDataValue);
		pCurrentData += sizeof(float);
	}

	return true;
}

int CDevice_FaceCap_Hardware::FetchData()
{
	int number_of_packets = 0;

	if (mSocket)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(mSocket, &readSet);
		struct timeval timeout = { 1, 0 }; // select times out after 1 second
		if (select(mSocket + 1, &readSet, NULL, NULL, &timeout) > 0) {
			sockaddr_in	lClientAddr;
			int		lSize;

			lSize = sizeof(lClientAddr);
			bzero((char *)&lClientAddr, sizeof(lClientAddr));

			int bytes_received = 1;

			while (bytes_received > 0)
			{
				memset(mBuffer, 0, sizeof(char) * 2048);

				bytes_received = recvfrom(mSocket, mBuffer, 2048, 0, (struct sockaddr*) &lClientAddr, &lSize);
				if (m_Verbose)
				{
					
					FBTrace("bytes received - %d\n", bytes_received);
				}

				if (bytes_received > 0)
				{
					if (ProcessMessage(mBuffer, bytes_received))
					{
						number_of_packets = 1;
					}
				}
			}
		}
	}

	return number_of_packets;
}


/************************************************
 *	Get the device setup information.
 ************************************************/
bool CDevice_FaceCap_Hardware::GetSetupInfo()
{
	return true;
}


/************************************************
 *	Start the device streaming mode.
 ************************************************/
bool CDevice_FaceCap_Hardware::StartStream()
{
	mSocket = StartServer(mNetworkPort);
	if (mSocket && m_Verbose)
	{
		FBTrace("mSocket - %d\n", mSocket);
	}
	
	return mSocket != 0;
}


/************************************************
 *	Stop the device streaming mode.
 ************************************************/
bool CDevice_FaceCap_Hardware::StopStream()
{
	if (mSocket) closesocket(mSocket);
	mSocket = 0;

	return true;
}



/************************************************
 *	Get the current position.
 ************************************************/
void CDevice_FaceCap_Hardware::GetPosition(double* pPos)
{
	pPos[0] = mPosition[0];
	pPos[1] = mPosition[1];
	pPos[2] = mPosition[2];
}


/************************************************
 *	Get the current rotation.
 ************************************************/
void CDevice_FaceCap_Hardware::GetRotation(double* pRot)
{
	pRot[0] = mRotation[0];
	pRot[1] = mRotation[1];
	pRot[2] = mRotation[2];
}

void CDevice_FaceCap_Hardware::GetLeftEyeRotation(double* rotation)
{
	rotation[0] = m_LeftEye[0];
	rotation[1] = m_LeftEye[1];
}
void CDevice_FaceCap_Hardware::GetRightEyeRotation(double* rotation)
{
	rotation[0] = m_RightEye[0];
	rotation[1] = m_RightEye[1];
}

const int CDevice_FaceCap_Hardware::GetNumberOfBlendshapes() const
{
	return static_cast<int>(EHardwareBlendshapes::count);
}
const double CDevice_FaceCap_Hardware::GetBlendshapeValue(const int index)
{
	return m_BlendShapes[index];
}

/************************************************
 *	Communications type.
 ************************************************/
void CDevice_FaceCap_Hardware::SetCommunicationType(FBCommType pType)
{
	mParent->CommType = pType;
}
int CDevice_FaceCap_Hardware::GetCommunicationType()
{
	return mParent->CommType;
}
