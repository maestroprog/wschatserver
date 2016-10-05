#ifndef PACKETS_H_
#define PACKETS_H_

#include <ctime>
#include <vector>

#include "packet.hpp"
#include "rooms.hpp"

using std::vector;

class PacketBad : public Packet {
public:
	virtual void deserialize(const Json::Value &){}
	virtual Json::Value serialize() const { return Json::Value(); }
	virtual void process(Client &){}
};

class PacketSystem : public Packet {
private:

public:
	string target;
	string message;
	
	PacketSystem();
	PacketSystem(const string &targ, const string &msg);
	virtual ~PacketSystem();
	
	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketMessage : public Packet {
private:
	bool processCommand(MemberPtr member, RoomPtr room, const string &msg);
public:
	time_t msgtime;
	string target;
	string login;
	string message;
	bool isprivate;

	PacketMessage();
	PacketMessage(const string &targ, const string &log, const string &msg) : PacketMessage(targ, log, msg, time(nullptr)){};
	PacketMessage(const string &targ, const string &log, const string &msg, const time_t &tm);
	virtual ~PacketMessage();
	
	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketOnlineList : public Packet {
private:

public:
	string target;
	Json::Value list;

	PacketOnlineList();
	virtual ~PacketOnlineList();
	
	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketAuth : public Packet {
private:

public:
	string ukey;
	
	PacketAuth();
	virtual ~PacketAuth();
	
	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketStatus : public Packet {
private:

public:
	string target;
	Member::Status status;
	string name;
	string data;
	
	PacketStatus();
	PacketStatus(RoomPtr room, MemberPtr member, Member::Status stat, const string &data = "");
	PacketStatus(RoomPtr room, MemberPtr member, const string &data = "");
	PacketStatus(const string &tg, const string &nm, Member::Status stat, const string &nname = "");
	virtual ~PacketStatus();
	
	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketJoin : public Packet {
private:

public:
	string target;

	PacketJoin();
	virtual ~PacketJoin();

	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

class PacketLeave : public Packet {
private:

public:
	string target;

	PacketLeave();
	virtual ~PacketLeave();

	virtual void deserialize(const Json::Value &);
	virtual Json::Value serialize() const;
	virtual void process(Client &);
};

#endif

