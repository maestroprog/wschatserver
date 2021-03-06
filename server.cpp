#include <vector>
#include <map>
#include <time.h>
#include <regex>

#include "algo.hpp"
#include "client.hpp"
#include "server.hpp"
#include "packets.hpp"
#include "logger.hpp"

Server::Server(int port)
	: server(config["ssl"]["certificate"].asString(), config["ssl"]["private_key"].asString())
{
	server.config.port = (unsigned short) port;
	server.config.thread_pool_size = 1;

	auto& chat = server.endpoint["^/chat/?$"];
	
	chat.on_message = [&](auto connection, auto message) {
		string msg = message->string();
		try {
			if (clients.find(connection) != clients.end()){
				clients[connection]->onPacket(msg);
			}
		} catch (const exception &e){
			Logger::error("Exception: ", e.what(), "\nWhile processing message:", msg);
		} catch (...){
			Logger::error("Unknown error while processing message:", msg);
		}
	};
	
	chat.on_open = [&, this](auto connection) {
		auto iphdr = connection->header.find("X-Real-IP");
		if (iphdr != connection->header.end()){
			connection->remote_endpoint_address = iphdr->second;
		}

		ClientPtr cli = make_shared<Client>(this, connection);
		cli->setSelfPtr(cli);

		Logger::info("Opened connection from ", cli->getIP());

		auto &cnt = connectionsCountFromIp[cli->getIP()];
		if (cnt >= 5){ //TODO: to config
			Logger::info("Connections limit reached for ", cli->getIP());
			server.send_close(connection, 0);
		}
		++cnt;

	    clients[connection] = cli;
	};
	
	chat.on_close = [&](auto connection, int status, const string& reason) {
	    Logger::info("Closed connection from ", connection->remote_endpoint_address, " with status code ", status);

		auto &cnt = connectionsCountFromIp[connection->remote_endpoint_address];
		--cnt;

		if (cnt <= 0){
			connectionsCountFromIp.erase(connection->remote_endpoint_address);
		}

	    if (clients.find(connection) != clients.end()){
			clients[connection]->onDisconnect();
			clients.erase(connection);
	    }
	};
	
	chat.on_error = [&](auto connection, const boost::system::error_code& ec) {
		Logger::warn("Error in connection from ", connection->remote_endpoint_address,
				". Error: ", ec, ", error message: ", ec.message());

		auto &cnt = connectionsCountFromIp[connection->remote_endpoint_address];
		--cnt;

		if (cnt <= 0){
			connectionsCountFromIp.erase(connection->remote_endpoint_address);
		}

		if (clients.find(connection) != clients.end()){
			clients[connection]->onDisconnect();
			clients.erase(connection);
		}
	};

	server.runWithInterval(pingInterval, [&]{
		time_t cur = time(nullptr);
		vector<ClientPtr> toKick;

		for (auto c : clients){
			auto &cli = c.second;

			if (cur - cli->lastPacketTime > connectTimeout){
				toKick.push_back(cli);
			}
			else if (cur - cli->lastPacketTime > pingTimeout){
				//cout << date("[%H:%M:%S] ") << "Server: Sending ping to " << cli->getName() << " [" << cli->getIP() << "]" << endl;
				cli->sendPacket(PacketPing());
			}
		}

		for (auto cli : toKick){
			kick(cli);
			Logger::info("Kicked by no ping: ", cli->getName(), " [", cli->getIP(), "]");
		}
	});
}

void Server::start(){
	Logger::info("Started wsserver at port ", server.config.port);
	connectionsCountFromIp.clear();
	server.start();
}

void Server::stop(){
	server.stop();
}

Json::Value Server::serialize(){
	Json::Value val;

	auto &sr = val["rooms"];
	for (RoomPtr r : rooms){
		sr.append(r->serialize());
	}

	return val;
}

void Server::deserialize(const Json::Value &val){
	rooms.clear();
	for (auto &v : val["rooms"]){
		RoomPtr rm = make_shared<Room>(this);
		rm->setSelfPtr(rm);
		rm->deserialize(v);
		rooms.insert(rm);
	}
}

void Server::sendRawData(shared_ptr<WSServerBase::Connection> conn, const string &rdata){
	auto response_ss = make_shared<SendStream>();
	*response_ss << rdata;
	server.send(conn, response_ss);
}

void Server::sendPacket(shared_ptr<WSServerBase::Connection> conn, const Packet &pack){
	Json::FastWriter wr;
	auto response_ss = make_shared<SendStream>();
	*response_ss << wr.write(pack.serialize());
	server.send(conn, response_ss);
}

void Server::sendPacketToAll(const Packet &pack){
	Json::FastWriter wr;
	auto response_ss = make_shared<SendStream>();
	string spack = wr.write(pack.serialize());

	*response_ss << spack;
	for (auto conn : server.get_connections()){
		//response_ss->seekg(0);
		server.send(conn, response_ss);
	}
}

ClientPtr Server::getClientByName(string name){
	for (auto clip : clients){
		auto cli = clip.second;
		if (!cli->getName().empty() && cli->getName() == name)
			return cli; 
	}
	return ClientPtr();
}

ClientPtr Server::getClientByID(uint uid){
	for (auto clip : clients){
		auto cli = clip.second;
		if (cli->getID() > 0 && cli->getID() == uid)
			return cli; 
	}
	return ClientPtr();
}

vector<ClientPtr> Server::getClients(){
	vector<ClientPtr> res;
	for (auto clip : clients){
		auto &cli = clip.second;
		res.push_back(cli);
	}

	return res;
}

void Server::kick(ClientPtr client){
	auto conn = client->getConnection();
	clients.erase(conn);
	client->onDisconnect();
	server.send_close(conn, 0);
}

RoomPtr Server::createRoom(string name){
	auto rm = getRoomByName(name);
	if (rm)
		return nullptr;

	rm = make_shared<Room>(this);
	rm->setName(name);
	rm->setSelfPtr(rm);
	rooms.insert(rm);
	rm->onCreate();

	return rm;
}

bool Server::removeRoom(string name){
	auto rm = getRoomByName(name);
	if (rm){
		bool res = rooms.erase(rm) > 0;
		if (res){
			rm->onDestroy();
		}
		return res;
	}

	return false;
}

RoomPtr Server::getRoomByName(string name){
	for (RoomPtr room : rooms){
		if (room->getName() == name){
			return room;
		}
	}

	return nullptr;
}

