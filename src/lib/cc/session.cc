
#include "data.h"
#include "session.h"

#include <cstdio>
#include <iostream>
#include <sstream>

using namespace std;
using namespace isc::cc;
using namespace isc::data;

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

Session::Session()
{
    sock = -1;
    sequence = 1;
}

void
Session::disconnect()
{
    close(sock);
    sock = -1;
}

void
Session::establish()
{
    int ret;
    struct sockaddr_in sin;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < -1)
        throw SessionError("socket() failed");

    sin.sin_family = AF_INET;
    sin.sin_port = htons(9912);
    sin.sin_addr.s_addr = INADDR_ANY;

#ifdef HAVE_SIN_LEN
    sin.sin_len = sizeof(struct sockaddr_in);
#endif

    ret = connect(sock, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0)
        throw SessionError("Unable to connect to message queue");

    //
    // send a request for our local name, and wait for a response
    //
    std::string get_lname_str = "{ \"type\": \"getlname\" }";
    std::stringstream get_lname_stream;
    get_lname_stream.str(get_lname_str);
    ElementPtr get_lname_msg = Element::createFromString(get_lname_stream);
    sendmsg(get_lname_msg);

    ElementPtr routing, msg;
    recvmsg(routing, msg, false);

    lname = msg->get("lname")->stringValue();
    cout << "My local name is:  " << lname << endl;
}

//
// Convert to wire format and send this on the TCP stream with its length prefix
//
void
Session::sendmsg(ElementPtr& msg)
{
    std::string header_wire = msg->toWire();
    unsigned int length = 2 + header_wire.length();
    unsigned int length_net = htonl(length);
    unsigned short header_length = header_wire.length();
    unsigned short header_length_net = htons(header_length);
    unsigned int ret;

    ret = write(sock, &length_net, 4);
    if (ret != 4)
        throw SessionError("Short write");

    ret = write(sock, &header_length_net, 2);
    if (ret != 2)
        throw SessionError("Short write");

    ret = write(sock, header_wire.c_str(), header_length);
    if (ret != header_length) {
        throw SessionError("Short write");
    }
}

void
Session::sendmsg(ElementPtr& env, ElementPtr& msg)
{
    std::string header_wire = env->toWire();
    std::string body_wire = msg->toWire();
    unsigned int length = 2 + header_wire.length() + body_wire.length();
    unsigned int length_net = htonl(length);
    unsigned short header_length = header_wire.length();
    unsigned short header_length_net = htons(header_length);
    unsigned int ret;

    ret = write(sock, &length_net, 4);
    if (ret != 4)
        throw SessionError("Short write");

    ret = write(sock, &header_length_net, 2);
    if (ret != 2)
        throw SessionError("Short write");

    ret = write(sock, header_wire.c_str(), header_length);
    if (ret != header_length) {
        throw SessionError("Short write");
    }
    ret = write(sock, body_wire.c_str(), body_wire.length());
    if (ret != body_wire.length()) {
        throw SessionError("Short write");
    }
}

bool
Session::recvmsg(ElementPtr& msg, bool nonblock)
{
    unsigned int length_net;
    unsigned short header_length_net;
    unsigned int ret;

    ret = read(sock, &length_net, 4);
    if (ret != 4)
        throw SessionError("Short read");

    ret = read(sock, &header_length_net, 2);
    if (ret != 2)
        throw SessionError("Short read");

    unsigned int length = ntohl(length_net) - 2;
    unsigned short header_length = ntohs(header_length_net);
    if (header_length != length) {
        throw SessionError("Received non-empty body where only a header expected");
    }
    
    char *buffer = new char[length];
    ret = read(sock, buffer, length);
    if (ret != length)
        throw SessionError("Short read");

    std::string wire = std::string(buffer, length);
    delete [] buffer;

    std::stringstream wire_stream;
    wire_stream <<wire;

    msg = Element::fromWire(wire_stream, length);

    return (true);
    // XXXMLG handle non-block here, and return false for short reads
}

bool
Session::recvmsg(ElementPtr& env, ElementPtr& msg, bool nonblock)
{
    unsigned int length_net;
    unsigned short header_length_net;
    unsigned int ret;

    ret = read(sock, &length_net, 4);
    if (ret != 4)
        throw SessionError("Short read");

    ret = read(sock, &header_length_net, 2);
    if (ret != 2)
        throw SessionError("Short read");

    unsigned int length = ntohl(length_net);
    unsigned short header_length = ntohs(header_length_net);
    if (header_length > length)
        throw SessionError("Bad header length");

    // remove the header-length bytes from the total length
    length -= 2;
    char *buffer = new char[length];
    ret = read(sock, buffer, length);
    if (ret != length)
        throw SessionError("Short read");

    std::string header_wire = std::string(buffer, header_length);
    std::string body_wire = std::string(buffer + header_length, length - header_length);
    delete [] buffer;

    std::stringstream header_wire_stream;
    header_wire_stream << header_wire;
    env = Element::fromWire(header_wire_stream, header_length);
    
    std::stringstream body_wire_stream;
    body_wire_stream << body_wire;
    msg = Element::fromWire(body_wire_stream, length - header_length);

    return (true);
    // XXXMLG handle non-block here, and return false for short reads
}

void
Session::subscribe(std::string group, std::string instance)
{
    ElementPtr env = Element::create(std::map<std::string, ElementPtr>());

    env->set("type", Element::create("subscribe"));
    env->set("group", Element::create(group));
    env->set("instance", Element::create(instance));

    sendmsg(env);
}

void
Session::unsubscribe(std::string group, std::string instance)
{
    ElementPtr env = Element::create(std::map<std::string, ElementPtr>());

    env->set("type", Element::create("unsubscribe"));
    env->set("group", Element::create(group));
    env->set("instance", Element::create(instance));

    sendmsg(env);
}

unsigned int
Session::group_sendmsg(ElementPtr msg, std::string group,
                       std::string instance, std::string to)
{
    ElementPtr env = Element::create(std::map<std::string, ElementPtr>());

    env->set("type", Element::create("send"));
    env->set("from", Element::create(lname));
    env->set("to", Element::create(to));
    env->set("group", Element::create(group));
    env->set("instance", Element::create(instance));
    env->set("seq", Element::create(sequence));
    //env->set("msg", Element::create(msg->toWire()));

    sendmsg(env, msg);

    return (sequence++);
}

bool
Session::group_recvmsg(ElementPtr& envelope, ElementPtr& msg, bool nonblock)
{
    bool got_message = recvmsg(envelope, msg, nonblock);
    if (!got_message) {
        return false;
    }

    return (true);
}

unsigned int
Session::reply(ElementPtr& envelope, ElementPtr& newmsg)
{
    ElementPtr env = Element::create(std::map<std::string, ElementPtr>());

    env->set("type", Element::create("send"));
    env->set("from", Element::create(lname));
    env->set("to", Element::create(envelope->get("from")->stringValue()));
    env->set("group", Element::create(envelope->get("group")->stringValue()));
    env->set("instance", Element::create(envelope->get("instance")->stringValue()));
    env->set("seq", Element::create(sequence));
    env->set("reply", Element::create(envelope->get("seq")->intValue()));

    sendmsg(env, newmsg);

    return (sequence++);
}