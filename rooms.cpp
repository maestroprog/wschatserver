#include "rooms.hpp"
#include "packets.hpp"
#include <ctime>

MemberInfo::MemberInfo(){
	user_id = 0;
}

MemberInfo::MemberInfo(MemberPtr member){
	user_id = member->getClient()->getID();
	nick = member->getNick();
	girl = member->isGirl();
	color = member->getColor();
}

Json::Value MemberInfo::serialize(){
	Json::Value v(Json::objectValue);

	v["user_id"] = user_id;
	v["nick"] = nick;
	v["girl"] = girl;
	v["color"] = color;

	return v;
}

void MemberInfo::deserialize(const Json::Value &val){
	user_id = val["user_id"].asUInt();
	nick = val["nick"].asString();
	girl = val["girl"].asBool();
	color = val["color"].asString();
}

void Member::sendPacket(const Packet &pack){
	client->sendPacket(pack);
}

void Member::setNick(const string &nnick){
	if (nick == nnick){
		return;
	}

	auto roomp = room.lock();

	if (!isModer() && roomp->isBannedNick(nnick)){
		sendPacket(PacketSystem(roomp->getName(), "Данный ник запрещен, выберите другой"));
		return;
	}

	string oldnick = nick;
	nick = nnick;

	PacketStatus spack(self.lock());
	if (!oldnick.empty()){
		if (nick.empty()){
			spack.status = Member::Status::offline;
			spack.name = oldnick;
		} else {
			spack.status = Member::Status::nick_change;
			spack.data = oldnick;
		}
	}

	roomp->sendPacketToAll(spack);

	if (!nick.empty()){
		Logger::info(roomp->getName(), ": Login = ", nick, " (", client->getIP(), ")");
	}
}

bool Member::isAdmin(){ return client->isAdmin(); }
bool Member::isOwner(){ return client->isAdmin() || (client->getID() != 0 && !room.expired() && client->getID() == room.lock()->getOwner()); }
bool Member::isModer(){ return isOwner() || (client->getID() != 0 && !room.expired() && room.lock()->isModerator(client->getID())); }


Room::Room(Server *srv){
	server = srv;
	ownerId = -1;
	nextMemberId = 0;
}

Room::~Room(){

}

void Room::onCreate(){

}

void Room::onDestroy(){
	auto mems = members;
	auto ptr = self.lock();
	for (MemberPtr m : mems){
		m->client->leaveRoom(ptr);
	}
}

Json::Value Room::serialize(){
	auto storeSet = [](auto &stg, const string &name, const auto &set){
		stg[name] = Json::Value(Json::arrayValue);
		auto &sval = stg[name];
		for (auto p : set){
			sval.append(p);
		}
	};

	Json::Value val;
	val["owner_id"] = ownerId;
	val["name"] = name;

	val["history"] = Json::Value(Json::arrayValue);
	auto &hist = val["history"];
	for (string p : history){
		hist.append(p);
	}

	val["members_info"] = Json::Value(Json::arrayValue);
	auto &mi = val["members_info"];
	for (auto &p : membersInfo){
		mi.append(p.second.serialize());
	}

	storeSet(val, "bannedNicks", bannedNicks);
	storeSet(val, "bannedIps", bannedIps);
	storeSet(val, "bannedUids", bannedUids);
	storeSet(val, "moderators", moderators);

	return val;
}

void Room::deserialize(const Json::Value &val){
	ownerId = val["owner_id"].asUInt();
	name = val["name"].asString();

	history.clear();
	for (auto &v : val["history"]){
		history.push_back(v.asString());
	}

	membersInfo.clear();
	for (auto &v : val["members_info"]){
		MemberInfo info;
		info.deserialize(v);
		membersInfo[info.user_id] = info;
	}

	bannedNicks.clear();
	for (auto &v : val["bannedNicks"]){
		bannedNicks.insert(v.asString());
	}

	bannedIps.clear();
	for (auto &v : val["bannedIps"]){
		bannedIps.insert(v.asString());
	}

	bannedUids.clear();
	for (auto &v : val["bannedUids"]){
		bannedUids.insert(v.asUInt());
	}

	moderators.clear();
	for (auto &v : val["moderators"]){
		moderators.insert(v.asUInt());
	}
}

uint Room::genNextMemberId(){
	do {
		++nextMemberId;
	} while (nextMemberId == 0 || findMemberById(nextMemberId));
	return nextMemberId;
}

void Room::addToHistory(const Packet &pack){
	if (pack.type == Packet::Type::message && ((const PacketMessage &) pack).to_id == 0){
		Json::FastWriter wr;
		history.push_back(wr.write(pack.serialize()));
		if (history.size() > 50){
			history.pop_front();
		}
	}
}

MemberPtr Room::findMemberByClient(ClientPtr client){
	for (MemberPtr m : members){
		if (m->client == client){
			return m;
		}
	}

	return nullptr;
}

MemberPtr Room::findMemberByNick(string nick){
	for (MemberPtr m : members){
		if (!m->nick.empty() && m->nick == nick){
			return m;
		}
	}

	return nullptr;
}

MemberPtr Room::findMemberById(uint id){
	for (MemberPtr m : members){
		if (m->id == id){
			return m;
		}
	}

	return nullptr;
}

void Room::setOwner(uint nid){
	ownerId = nid;
}

MemberPtr Room::addMember(ClientPtr user){
	auto ptr = self.lock();
	auto m = make_shared<Member>(ptr, user);
	m->setSelfPtr(m);
	m->id = genNextMemberId();

	if (!m->isModer()){
		if (bannedIps.find(user->getIP()) != bannedIps.end()){
			user->sendPacket(PacketSystem("", "Вы были забанены"));
			return nullptr;
		}
		else if (bannedUids.find(user->getID()) != bannedUids.end()){
			user->sendPacket(PacketSystem("", user->getID() == 0 ? "Гости не могут войти в эту комнату. Авторизуйтесь на сайте" : "Вы были забанены"));
			return nullptr;
		}
	}

	auto res = members.insert(m);

	user->sendPacket(PacketJoin(m));
	user->sendPacket(PacketOnlineList(self.lock()));

	if (res.second){
		return *res.first;
	}

	return nullptr;
}

bool Room::removeMember(ClientPtr user){
	auto m = findMemberByClient(user);
	if (!m->getNick().empty()){
		sendPacketToAll(PacketStatus(m, Member::Status::offline));
	}

	user->sendPacket(PacketLeave(name));

	auto cli = m->getClient();
	if (!cli->isGuest()){
		membersInfo[cli->getID()] = MemberInfo(m);
	}

	return members.erase(m) > 0;
}

MemberInfo Room::getStoredMemberInfo(MemberPtr member){
	uint uid = member->getClient()->getID();
	if (uid == 0){
		return MemberInfo();
	}

	auto mi = membersInfo.find(uid);
	if (mi == membersInfo.end()){
		return MemberInfo();
	}

	return mi->second;
}

bool Room::kickMember(ClientPtr user, string reason){
	auto m = findMemberByClient(user);
	if (m){
		return kickMember(m, reason);
	}

	return false;
}

bool Room::kickMember(MemberPtr member, string reason){
	auto ptr = self.lock();
	member->getClient()->onKick(ptr);
	if (!member->getNick().empty()){
		sendPacketToAll(PacketStatus(member, Member::Status::offline));
	}

	member->sendPacket(PacketLeave(name));

	return members.erase(member) > 0;
}

void Room::sendPacketToAll(const Packet &pack){
	addToHistory(pack);
	for (MemberPtr m : members){
		m->getClient()->sendPacket(pack);
	}
}

