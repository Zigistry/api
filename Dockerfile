FROM fedora:latest

WORKDIR /workplace

RUN dnf install -y wget libmicrohttpd unzip

RUN mkdir include

RUN wget -O server https://github.com/Zigistry/api/releases/download/api-binary/server

RUN wget -O ./a.zip https://github.com/tursodatabase/libsql-c/releases/download/v0.3.4/x86_64-unknown-linux-gnu-release.zip

RUN wget -O ./zigistry.db https://huggingface.co/buckets/Zigistry/Zigistry/resolve/zigistry.db?download=true

RUN unzip ./a.zip

RUN cp ./liblibsql.so ./include/

RUN chmod +x ./server 

ENV LD_LIBRARY_PATH=/workplace/include:$LD_LIBRARY_PATH

EXPOSE 7860

COPY start.sh .
RUN chmod +x start.sh

CMD ["./start.sh"]
