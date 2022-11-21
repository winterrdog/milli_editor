milli: src/milli.c
	@${CC} -Wall -Wextra -pedantic -Ofast -flto -o $@ -std=c17 $<

debug: src/milli.c
	@${CC} -Wall -Wextra -pedantic -ggdb3 -Og -o $@ -std=c17 $<

clean:
	@rm -rf milli debug test
