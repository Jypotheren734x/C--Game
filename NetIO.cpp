#include "NetIO.h"
#include "boost\asio.hpp"
#include <iostream>

using namespace std;
using namespace boost::asio;

io_service ioservice;

ip::tcp::acceptor acceptor(ioservice);
ip::tcp::socket sock(ioservice);

bool server;

ip::tcp::endpoint endpoint;
void netio::startServer() {
	server = true;
	try {
		endpoint = ip::tcp::endpoint(ip::tcp::v4(), 7777);
		acceptor.open(endpoint.protocol());
		acceptor.set_option(ip::tcp::acceptor::reuse_address(false));
		acceptor.bind(endpoint);
		acceptor.listen(socket_base::max_connections);
		acceptor.accept(sock);
		
		cout << "Listening on " << endpoint << endl;
		started = true;
	} catch (exception & ex) {
		cerr << "Exception: " << ex.what() << endl;
	}
}

void netio::stopServer() {
	boost::system::error_code ec;
	acceptor.close(ec);
	sock.shutdown(ip::tcp::socket::shutdown_both, ec);
	sock.close(ec);

	ioservice.stop();
}


void netio::startClient(string serverAddr) {
	server = false;
	try {
		endpoint = *ip::tcp::resolver(ioservice).resolve(ip::tcp::resolver::query(
			serverAddr,
			"7777"
		));
		boost::system::error_code ec;
		sock.connect(endpoint, ec);
		
		if (!ec) {
			cout << "Connected on " << endpoint << endl;
			started = true;
		}
	} catch (exception & ex) {
		cerr << "Exception: " << ex.what() << endl;
	}
}

string netio::getLocalAddress() {
	try {
		ip::tcp::resolver::iterator it = ip::tcp::resolver(ioservice).resolve(ip::tcp::resolver::query(ip::host_name(),""));
		
		while (it!=ip::tcp::resolver::iterator()) {
			ip::address addr=(it++)->endpoint().address();
			if (addr.is_v4())
				return addr.to_string();
		}
		
		return "Could not determine";
	} catch (exception ex) {
		return "An error occurred";
	}
}

void netio::stopClient() {
	boost::system::error_code ec;
	sock.shutdown(ip::tcp::socket::shutdown_both, ec);
	sock.close(ec);

	ioservice.stop();
}


void netio::writeFloat(float data) {
	try {
		sock.write_some(buffer(&data, sizeof(float)));
	} catch (exception ex) {}
}

float netio::readFloat() {
	float data = 0;
	try {
		sock.read_some(buffer(&data, sizeof(float)));
	} catch (exception ex) {}
	return data;
}

void netio::writeUInt(unsigned data) {
	try {
		sock.write_some(buffer(&data, sizeof(unsigned)));
	} catch (exception ex) {}
}

unsigned netio::readUInt() {
	unsigned data = 0;
	try {
		sock.read_some(buffer(&data, sizeof(unsigned)));
	} catch (exception ex) {}
	return data;
}

void netio::stop() {
	if (server)
		stopServer();
	else
		stopClient();
}

bool netio::started = false;