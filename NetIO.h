#pragma once

#include <string>
using std::string;

namespace netio {
	
	// server
	void startServer();
	void stopServer();
	
	// client
	void startClient(string serverAddr);
	void stopClient();
	string getLocalAddress();

	// both
	void writeFloat(float data);
	float readFloat();
	void writeUInt(unsigned data);
	unsigned readUInt();
	void stop();
	extern bool started;

}