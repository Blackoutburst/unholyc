#include <catch2/catch_all.hpp>
#include <uhcstd.hh>

// ──────────────────────────────────────────────────────────────────────────────
// Mutex
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Mutex create/lock/unlock/destroy", "[mutex]") {
    Mutex::It m = Mutex::create();
    Mutex::lock(m);
    Mutex::unlock(m);
    Mutex::destroy(m);
    SUCCEED("no crash");
}

// ──────────────────────────────────────────────────────────────────────────────
// Thread + Mutex — shared counter
// ──────────────────────────────────────────────────────────────────────────────

static Mutex::It g_mutex;
static int       g_counter = 0;
static int       g_iterations = 0;

static void* workerFn(void* arg) {
    (void)arg;
    for (int i = 0; i < g_iterations; i++) {
        Mutex::lock(g_mutex);
        g_counter++;
        Mutex::unlock(g_mutex);
    }
    return nullptr;
}

TEST_CASE("Thread two workers increment shared counter", "[thread]") {
    g_mutex     = Mutex::create();
    g_counter   = 0;
    g_iterations = 5;

    int id1 = 1, id2 = 2;
    Thread::It t1 = Thread::start(workerFn, &id1);
    Thread::It t2 = Thread::start(workerFn, &id2);
    Thread::join(t1);
    Thread::join(t2);

    CHECK(g_counter == 10);
    Mutex::destroy(g_mutex);
}

TEST_CASE("Thread four workers increment shared counter", "[thread]") {
    g_mutex     = Mutex::create();
    g_counter   = 0;
    g_iterations = 100;

    int ids[4] = {0, 1, 2, 3};
    Thread::It threads[4];
    for (int i = 0; i < 4; i++)
        threads[i] = Thread::start(workerFn, &ids[i]);
    for (int i = 0; i < 4; i++)
        Thread::join(threads[i]);

    CHECK(g_counter == 400);
    Mutex::destroy(g_mutex);
}
