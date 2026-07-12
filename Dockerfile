FROM fedora:latest

WORKDIR /workplace

RUN dnf install -y wget unzip sqlite

RUN wget -O server https://github.com/Zigistry/api/releases/download/api-binary/server

RUN wget -O ./zigistry.db https://huggingface.co/buckets/Zigistry/Zigistry/resolve/zigistry.db?download=true

RUN chmod +x ./server 

EXPOSE 7860

COPY start.sh .
RUN chmod +x start.sh

CMD ["./start.sh"]
