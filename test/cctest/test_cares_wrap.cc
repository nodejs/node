#include "cares_wrap.h"

#include "gtest/gtest.h"
#include "node.h"
#include "node_test_fixture.h"
#include "v8.h"

#include <winsock2.h>

class CaresWrapTest : public EnvironmentTestFixture {};

TEST_F(CaresWrapTest, FreshChannelChecksServersBeforeFirstQuery) {
  v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  v8::Local<v8::FunctionTemplate> templ =
      node::BaseObject::MakeLazilyInitializedJSTemplate(*env);
  v8::Local<v8::Object> object = templ->GetFunction(env.context())
                                     .ToLocalChecked()
                                     ->NewInstance(env.context())
                                     .ToLocalChecked();

  node::cares_wrap::ChannelWrap channel(*env, object, -1, 4, 0);

  ares_addr_port_node server{};
  server.family = AF_INET;
  server.addr.addr4.s_addr = htonl(INADDR_LOOPBACK);
  server.tcp_port = 12345;
  server.udp_port = 12345;

  ASSERT_EQ(ares_set_servers_ports(channel.cares_channel(), &server),
            ARES_SUCCESS);
  channel.set_is_servers_default(true);

  channel.EnsureServers();

  ares_addr_port_node* current = nullptr;
  ares_get_servers_ports(channel.cares_channel(), &current);
  ASSERT_NE(current, nullptr);

  const bool still_using_test_loopback =
      current->next == nullptr && current->family == AF_INET &&
      current->addr.addr4.s_addr == htonl(INADDR_LOOPBACK) &&
      current->tcp_port == 12345 && current->udp_port == 12345;

  ares_free_data(current);

  EXPECT_FALSE(still_using_test_loopback);
}
