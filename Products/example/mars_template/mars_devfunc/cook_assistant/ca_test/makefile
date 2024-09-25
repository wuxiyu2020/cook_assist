
# app = $(wildcard ../buildin_signals.c ../ca_apps.c ../ca_signals_api.c ../ca_signals_lib.c ../app_pot_cover/*.c ../app_water_boil/*.c ../app_quick_cooking/*.c ../*.c ./*c) 
app = $(wildcard ../app_pot_cover/*.c ../app_water_boil/*.c ../app_quick_cooking/*.c ../*.c ./*c) 
OBJ = $(patsubst %.c,%.o,$(app))

# OBJ = cloud.o fsyd.o mlog.o ring_buffer.o ca_lib.o pot_cover.o signals.o signals_pot_cover.o quick_cooking.o buildin_signals.o \
#  	 ca_test.o

all:ca_test clean

ca_test:$(OBJ)
	gcc $(OBJ) -g -lm -o ca_test

%.o:%.c
	gcc $^ -c -Wall -g -o $@

.PHONY:clean

clean:
	rm -f ./*.o
	rm -f ../*.o
	rm -f ../app_pot_cover/*.o ../app_water_boil/*.o ../app_quick_cooking/*.o
