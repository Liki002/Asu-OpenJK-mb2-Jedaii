// Throwaway copy of the account service with an in-memory store, for testing the engine client.
// usage: node test_service.js <port> [botdir]
const botDir = process.argv[3] || '.';
const { AccountsService, MemStore, createTcpServer } = require(botDir + '/accounts_core');
const svc = new AccountsService({ store: new MemStore(), secrets: { eu: 'eu-secret', us: 'us-secret' } });
createTcpServer(svc).listen(parseInt(process.argv[2], 10), '127.0.0.1').then((p) => console.log('test service listening on', p));
