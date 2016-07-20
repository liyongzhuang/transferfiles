CC = g++
NAME = odin_client
LIBS= -lpthread -lcrypto -lssl `pkg-config --libs glib-2.0` `pkg-config --cflags glib-2.0`

$(NAME):
	$(CC) client.cpp ../common/common.cpp ../common/confoperation.cpp ../common/writelog.cpp -o $(NAME) $(LIBS) -O2
.PHONY: clean
clean:
	-rm $(NAME) *.log *.ini 
