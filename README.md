# tcp-sm
TCP socket monitor CLI tool similar to `ss` command. `tcp-sm` works by outputing
the socket stats in regular intervals (default 2s).  Compared to `ss` it offers
more human readable output (conversion from raw bytes to KB, MB, GB) and
computation of socket network bandwidth.

## Usage
```
$ tcp-sm
ESTAB         10.0.0.8:37011  ->  147.161.139.252:443    R-Q:0 W-Q:0 pmtu:1500 mss:1440 cwnd:10 rtt:37.45/18.61 app_limited:0 received:27.14M sent:1.73M retrans:265.00B delivery_rate:729.44K send_rate:0.00B/s recv_rate:0.00B/s
ESTAB         10.0.0.8:35410  ->     37.0.113.110:443    R-Q:0 W-Q:0 pmtu:1500 mss:1452 cwnd:10 rtt:56.07/36.59 app_limited:1 received:182.09M sent:462.67M retrans:183.26K delivery_rate:6.47M send_rate:0.00B/s recv_rate:0.00B/s
ESTAB         10.0.0.8:37011  ->  147.161.139.252:443    R-Q:0 W-Q:0 pmtu:1500 mss:1440 cwnd:10 rtt:37.45/18.61 app_limited:0 received:27.14M sent:1.73M retrans:265.00B delivery_rate:729.44K send_rate:0.00B/s recv_rate:0.00B/s
ESTAB         10.0.0.8:35410  ->     37.0.113.110:443    R-Q:0 W-Q:0 pmtu:1500 mss:1452 cwnd:10 rtt:50.24/36.90 app_limited:1 received:182.09M sent:462.67M retrans:183.26K delivery_rate:6.47M send_rate:18.50B/s recv_rate:18.50B/s
ESTAB         10.0.0.8:37011  ->  147.161.139.252:443    R-Q:0 W-Q:0 pmtu:1500 mss:1440 cwnd:10 rtt:37.45/18.61 app_limited:0 received:27.14M sent:1.73M retrans:265.00B delivery_rate:729.44K send_rate:0.00B/s recv_rate:0.00B/s
ESTAB         10.0.0.8:35410  ->     37.0.113.110:443    R-Q:0 W-Q:0 pmtu:1500 mss:1452 cwnd:10 rtt:55.80/38.79 app_limited:1 received:182.09M sent:462.67M retrans:183.26K delivery_rate:6.47M send_rate:67.00B/s recv_rate:61.00B/s
ESTAB         10.0.0.8:37011  ->  147.161.139.252:443    R-Q:0 W-Q:0 pmtu:1500 mss:1440 cwnd:10 rtt:37.45/18.61 app_limited:0 received:27.14M sent:1.73M retrans:265.00B delivery_rate:729.44K send_rate:0.00B/s recv_rate:0.00B/s
ESTAB         10.0.0.8:35410  ->     37.0.113.110:443    R-Q:0 W-Q:0 pmtu:1500 mss:1452 cwnd:10 rtt:64.63/36.89 app_limited:1 received:182.09M sent:462.67M retrans:183.26K delivery_rate:6.47M send_rate:390.00B/s recv_rate:180.50B/s
```

## Install
Download the latest version and unpack the archive.
```
wget https://github.com/jsemric/tcp-sm/releases/download/1.0.0/tcp-sm-1.0.0.tar.gz && \
    tar xzvf tcp-sm-1.0.0.tar.gz -C /usr/local/bin
```
Currently only AMD debian-based distros are supported.

## Testing
First download googletest repo.
```
git clone https://github.com/google/googletest.git --branch v1.17.0 --depth 1
```
Once cloned, run the tests with make
```
make test
```

