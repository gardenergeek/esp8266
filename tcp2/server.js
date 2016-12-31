var net = require('net');

var server = net.createServer(function(socket) {
	console.log(`Apa`);	
	
	socket.write('Echo server\r\n');
	socket.pipe(socket);
});



server.listen(2000, '192.168.0.106');
