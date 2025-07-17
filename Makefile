CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread
INCLUDES = -Iinclude

SRC_DIR = src
BUILD_DIR = build

MANAGER_SRC = $(SRC_DIR)/manager/manager.cpp
NODE_AGENT_SRC = $(SRC_DIR)/node/node_agent.cpp
CLIENT_SRC = $(SRC_DIR)/client/client.cpp

MANAGER_BIN = $(BUILD_DIR)/manager
NODE_AGENT_BIN = $(BUILD_DIR)/node_agent
CLIENT_BIN = $(BUILD_DIR)/client

all: $(MANAGER_BIN) $(NODE_AGENT_BIN) $(CLIENT_BIN)

$(MANAGER_BIN): $(MANAGER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(NODE_AGENT_BIN): $(NODE_AGENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

clean:
	rm -f $(BUILD_DIR)/*
