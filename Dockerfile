FROM debian:stable-slim

RUN apt update && apt install -y libpoppler-glib-dev poppler-utils libwxgtk3.0-gtk3-dev automake g++ make build-essential && apt clean


WORKDIR /app
COPY . .
RUN ./bootstrap && ./configure --disable-dependency-tracking  && make && make install
ENTRYPOINT ["diff-pdf"]
