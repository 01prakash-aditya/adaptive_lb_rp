FROM golang:1.21-alpine

WORKDIR /app
COPY main.go .

RUN go build -o load_tester main.go

ENTRYPOINT ["./load_tester"]
