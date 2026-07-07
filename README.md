# Zigistry API

- **To run this API service:**

```sh
git clone git@github.com:Zigistry/api.git
cd api
```

- **Then we need to download the release of the latest libsql library from turso**

```sh
wget -O ./a.zip https://github.com/tursodatabase/libsql-c/releases/download/v0.3.4/aarch64-apple-darwin-debug.zip

wget -O ./zigistry.db https://huggingface.co/buckets/Zigistry/Zigistry/resolve/zigistry.db?download=true

unzip ./a.zip

mv ./liblibsql.a ./include/libsql.a
```

- ****
