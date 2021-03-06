CC = g++
CPPFLAGS = -Wall -O3 -std=c++1y -flto

SOURCES = $(wildcard *.cpp) $(wildcard regex/*.cpp)
OBJECTS = $(SOURCES:%.cpp=%.o)

APP_NAME = wsserver
APP = $(APP_NAME)

all: LDLIBS = -lpthread -lboost_system -lcrypto -lmysqlcppconn -lmemcached -ljsoncpp -lssl
all: $(APP)
	strip $(APP)

static_boost: LDLIBS = -lpthread -Wl,-Bstatic -lboost_system -Wl,-Bdynamic -lcrypto -lmysqlcppconn -lmemcached -ljsoncpp -lssl
static_boost: $(APP)
	strip $(APP)

debug: CPPFLAGS = -D_DEBUG_ -Wall -g3 -std=c++1y
debug: $(OBJECTS)
	$(LINK.o) $^ $(LDLIBS) -o $(APP)

clean:
	rm -f $(APP) $(OBJECTS)

$(APP): $(OBJECTS)
	$(LINK.o) -flto $^ $(LDLIBS) -o $(APP)
