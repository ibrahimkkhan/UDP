# UDP
This project describes a UDP client and server to run a simplified version of NTP
(Network Time Protocol). 
UDP provides fewer properties and guarantees
on top of IP. It also supports 216 ports that serve as communication endpoints on a
given host (which is identified by the IP address). UDP is a connection-less protocol,
meaning that a source can send data to a destination without first participating in a “handshaking”
protocol. Additionally, UDP does not handle streams of data, but rather individual messages
which are termed “datagrams”. 

Finally, UDP does not provide the following guarantees: 
1) that datagrams will be delivered
2) that the datagrams will be delivered in order
3) that the datagrams will be delivered without duplicates
