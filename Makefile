
INCLUDE_DIR = -I. -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include

SOURCES = main.c \
          utils.c \
          kitchen.c \
          input.c \
          shelf.c \
          courier.c \
          monitor.c 

OBJECTS := $(notdir $(SOURCES:.c=.o))

all : $(OBJECTS)
	$(CC) $(OBJECTS) -o css -lpthread -lglib-2.0

%.o : %.c
	$(CC) -g $(CFLAGS) $(INCLUDE_DIR) -o $@ -c $<
