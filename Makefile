CC = gcc
CFLAGS = -Wall -g
OBJDIR = bin
OBJS = $(OBJDIR)/main.o $(OBJDIR)/parser.o $(OBJDIR)/reader.o $(OBJDIR)/table.o  $(OBJDIR)/code_writer.o
DEPS = parser.h reader.h table.h code_writer.h
TARGET = $(OBJDIR)/translator

$(OBJDIR)/%.o: %.c $(DEPS)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -f $(OBJDIR)/*.o $(TARGET)
