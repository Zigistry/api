#!/bin/sh

update_db() {
  while true; do
    echo "Updating database..."

    wget -O /workplace/zigistry.db.new "https://huggingface.co/buckets/Zigistry/Zigistry/resolve/zigistry.db?download=true"

    mv /workplace/zigistry.db.new /workplace/zigistry.db

    sleep 3600
  done
}

# I will start this function in the background, it will download new db every hour
# but I doubt if the server running will use this new db.
# will fix this soon.
update_db &

exec ./server
