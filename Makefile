PJDIR = pjproject-2.15.1
include $(PJDIR)/build.mak

sipcall: sipcall.o
	$(PJ_CC) -o $@ $< $(PJ_LDFLAGS) $(PJ_LDLIBS)

sipcall.o: sipcall.c
	$(PJ_CC) -c -o $@ $< $(PJ_CFLAGS)

clean:
	rm -f sipcall.o sipcall
