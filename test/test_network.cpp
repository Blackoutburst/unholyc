#include <catch2/catch_all.hpp>
#include <uhcnet.hh>
#include <uhcstd.hh>
#include <atomic>
#include <string>
#include <string.h>

// Distinct ports per test to avoid TIME_WAIT conflicts on rapid re-runs.
static const short PORT_ECHO    = 54320;
static const short PORT_NOLIST  = 54321;  // nothing will listen here
static const short PORT_NOCONN  = 54322;  // bind + destroy only, no connections

// ── shared echo server state ─────────────────────────────────────────────────

static TcpServer::It    g_server;
static std::atomic<int> g_echo_count{0};
static char             g_echo_payload[8] = {0};

static void echo_handler(TcpClient::It& client) {
    char buf[8] = {0};
    if (TcpServer::readAll(client, buf, 4)) {
        TcpServer::write(client, buf, 4);
        memcpy(g_echo_payload, buf, 4);
        g_echo_count++;
    }
    TcpClient::destroy(client);
}

static void* server_fn(void* arg) {
    TcpServer::It* srv = static_cast<TcpServer::It*>(arg);
    TcpServer::listen(*srv, echo_handler);
    return nullptr;
}

// ── tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("TcpServer::create / destroy no crash", "[network]") {
    TcpServer::It srv = TcpServer::create(PORT_NOCONN, 1);
    REQUIRE(srv.sockfd >= 0);
    CHECK(srv.running != 0);
    TcpServer::destroy(srv);
}

TEST_CASE("TcpClient connect to closed port fails", "[network]") {
    TcpClient::It c = TcpClient::create((char*)"127.0.0.1", PORT_NOLIST);
    CHECK(c.sockfd < 0);
}

TEST_CASE("TCP loopback echo single message", "[network]") {
    g_echo_count = 0;
    memset(g_echo_payload, 0, sizeof(g_echo_payload));

    g_server = TcpServer::create(PORT_ECHO, 4);
    REQUIRE(g_server.sockfd >= 0);
    Thread::It srv_thread = Thread::start(server_fn, &g_server);
    Time::sleep(50);  // let server reach accept()

    TcpClient::It client = TcpClient::create((char*)"127.0.0.1", PORT_ECHO);
    REQUIRE(client.sockfd >= 0);

    unsigned char wok = TcpClient::write(client, "PING", 4);
    CHECK(wok != 0);

    char resp[5] = {0};
    unsigned char rok = TcpClient::readAll(client, resp, 4);
    REQUIRE(rok != 0);
    CHECK(std::string(resp, 4) == "PING");

    TcpClient::destroy(client);
    Time::sleep(100);  // let echo_handler finish

    CHECK(g_echo_count.load() == 1);
    CHECK(std::string(g_echo_payload, 4) == "PING");

    TcpServer::destroy(g_server);
    Thread::join(srv_thread);
}

TEST_CASE("TCP loopback echo multiple sequential clients", "[network]") {
    static TcpServer::It srv2;
    static std::atomic<int> count{0};

    srv2 = TcpServer::create(PORT_ECHO + 1, 4);
    REQUIRE(srv2.sockfd >= 0);

    static void (*multi_handler)(TcpClient::It&) = [](TcpClient::It& client) {
        char buf[4] = {0};
        if (TcpServer::readAll(client, buf, 4))
            TcpServer::write(client, buf, 4);
        count++;
        TcpClient::destroy(client);
    };

    static TcpServer::It* srv2_ptr = &srv2;
    Thread::It t = Thread::start([](void*) -> void* {
        TcpServer::listen(*srv2_ptr, multi_handler);
        return nullptr;
    }, nullptr);
    Time::sleep(50);

    for (int i = 0; i < 3; i++) {
        TcpClient::It c = TcpClient::create((char*)"127.0.0.1", PORT_ECHO + 1);
        REQUIRE(c.sockfd >= 0);
        TcpClient::write(c, "DATA", 4);
        char r[5] = {0};
        REQUIRE(TcpClient::readAll(c, r, 4) != 0);
        CHECK(std::string(r, 4) == "DATA");
        TcpClient::destroy(c);
        Time::sleep(20);
    }

    Time::sleep(100);
    CHECK(count.load() == 3);

    TcpServer::destroy(srv2);
    Thread::join(t);
}
