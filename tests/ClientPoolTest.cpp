#include <gtest/gtest.h>
#include "ClientPool.h"

// ---- ClientPool basic state ----

TEST(ClientPoolTest, InitiallyEmpty) {
    ClientPool pool;
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.size(), 0u);
}

TEST(ClientPoolTest, AddClientIncreasesSize) {
    ClientPool pool;
    pool.addClient(1, "alice");
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_FALSE(pool.empty());
}

TEST(ClientPoolTest, AddMultipleClients) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.addClient(2, "bob");
    EXPECT_EQ(pool.size(), 2u);
}

// ---- Username / socket existence ----

TEST(ClientPoolTest, UsernameExistsAfterAdd) {
    ClientPool pool;
    pool.addClient(1, "alice");
    EXPECT_TRUE(pool.usernameExists("alice"));
    EXPECT_FALSE(pool.usernameExists("bob"));
}

TEST(ClientPoolTest, SocketExistsAfterAdd) {
    ClientPool pool;
    pool.addClient(1, "alice");
    EXPECT_TRUE(pool.socketExists(1));
    EXPECT_FALSE(pool.socketExists(99));
}

// ---- Lookup ----

TEST(ClientPoolTest, FindBySocketReturnsCorrectInfo) {
    ClientPool pool;
    pool.addClient(42, "alice");
    auto result = pool.findBySocket(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->socketFd, 42);
    EXPECT_EQ(result->username, "alice");
}

TEST(ClientPoolTest, FindBySocketMissingReturnsNullopt) {
    ClientPool pool;
    EXPECT_FALSE(pool.findBySocket(99).has_value());
}

TEST(ClientPoolTest, FindByUsernameReturnsCorrectInfo) {
    ClientPool pool;
    pool.addClient(7, "bob");
    auto result = pool.findByUsername("bob");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->socketFd, 7);
}

TEST(ClientPoolTest, FindByUsernameMissingReturnsNullopt) {
    ClientPool pool;
    EXPECT_FALSE(pool.findByUsername("nobody").has_value());
}

// ---- Removal ----

TEST(ClientPoolTest, RemoveBySocketDecreasesSize) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.removeBySocket(1);
    EXPECT_TRUE(pool.empty());
}

TEST(ClientPoolTest, RemoveByUsernameDecreasesSize) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.removeByUsername("alice");
    EXPECT_TRUE(pool.empty());
}

TEST(ClientPoolTest, RemoveBySocketLeaveOthersIntact) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.addClient(2, "bob");
    pool.removeBySocket(1);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_TRUE(pool.usernameExists("bob"));
    EXPECT_FALSE(pool.usernameExists("alice"));
}

// ---- Snapshot ----

TEST(ClientPoolTest, GetUsernamesReturnsAll) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.addClient(2, "bob");
    auto names = pool.getUsernames();
    EXPECT_EQ(names.size(), 2u);
}

TEST(ClientPoolTest, GetClientsReturnsAll) {
    ClientPool pool;
    pool.addClient(1, "alice");
    pool.addClient(2, "bob");
    auto clients = pool.getClients();
    EXPECT_EQ(clients.size(), 2u);
}
