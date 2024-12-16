#!/bin/bash

docker run --rm -p 3001:80 -v ./:/usr/share/nginx/html:ro nginx