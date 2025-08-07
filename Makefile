NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include
COMMANDS_DIR = $(SRC_DIR)/commands

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/Server.cpp \
       $(SRC_DIR)/Client.cpp \
       $(SRC_DIR)/Channel.cpp \
       $(SRC_DIR)/Command.cpp \
       $(SRC_DIR)/Parser.cpp \
       $(SRC_DIR)/Utils.cpp \
       $(COMMANDS_DIR)/AuthCommands.cpp \
       $(COMMANDS_DIR)/ChannelCommands.cpp \
       $(COMMANDS_DIR)/MessageCommands.cpp \
       $(COMMANDS_DIR)/OperCommands.cpp \
       $(COMMANDS_DIR)/UtilityCommands.cpp \
       $(SRC_DIR)/bonus/Bot.cpp \
       $(SRC_DIR)/bonus/BotManager.cpp \
       $(SRC_DIR)/bonus/JankenBot.cpp

# Generate object file paths from source paths, maintaining structure
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "$(NAME) created successfully!"

# Compile rule: create object dir before compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)
	@echo "Object files cleaned."

fclean: clean
	rm -f $(NAME)
	@echo "Executable cleaned."

re: fclean all

.PHONY: all clean fclean re
