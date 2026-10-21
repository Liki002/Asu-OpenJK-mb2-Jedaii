// Throwaway copy of the account service with an in-memory store, for testing the engine client.
// usage: node test_service.js <port> [botdir] [--seed]
// --seed pre-creates three accounts (alice_seed, bob_seed, cara_seed) with stats, as server "us", so leaderboards,
// offline lookups and "name already taken" can be tested.
const botDir = process.argv[3] && !process.argv[3].startsWith('--') ? process.argv[3] : '.';
const { AccountsService, MemStore, createTcpServer } = require(botDir + '/accounts_core');
const svc = new AccountsService({ store: new MemStore(), secrets: { eu: 'eu-secret', us: 'us-secret' } });

async function seed() {
    const rows = [
        ['alice_seed', 'Alice', { elo: 1400, wins: 20, losses: 5, kills: 90, deaths: 40, streak: 7, highest_streak: 9 }, 500, 12],
        ['bob_seed', 'Bob', { elo: 1250, wins: 10, losses: 10, kills: 50, deaths: 50, streak: 3, highest_streak: 4 }, 900, 30],
        ['cara_seed', 'Cara', { elo: 1100, wins: 2, losses: 8, kills: 10, deaths: 30, streak: 0, highest_streak: 1 }, 100, 5]
    ];
    for (const [name, display, duel, credits, potato] of rows) {
        const reg = await svc.handle('us', { op: 'register', guid: `SEED-${name}`, displayName: display });
        await svc.handle('us', { op: 'rename', username: reg.username, newUsername: name });
        await svc.handle('us', { op: 'put', username: name, data: { ...reg.account, username: name, credits, max_potato_ticks: potato, xp: 2500, modes: { duel } } });
        await svc.handle('us', { op: 'release', username: name });
    }
    console.log('seeded 3 accounts');
}

createTcpServer(svc).listen(parseInt(process.argv[2], 10), '127.0.0.1').then(async (p) => {
    if (process.argv.includes('--seed')) await seed();
    console.log('test service listening on', p);
});
