[linux]
build:
  g++ -g -Wall -o main -I src src/*.cpp

[linux]
run: build
  ./main

[windows]
build:
  g++ -g -Wall -static -o main.exe -I src src/*.cpp

[windows]
run: build
  ./main.exe