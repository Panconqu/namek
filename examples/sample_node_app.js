const path = require('path');
const { NamekDB, CLIBuilder, Tools } = require(path.join(__dirname, '../sdk/node'));

console.log('\x1b[36m\x1b[1m=== NAMEK NODE.JS TOOLBOX DEMO ===\x1b[0m\n');

// 1. NamekDB Demo
const db = new NamekDB('demo_node_db.json');
db.set('server_status', 'active');
db.set('port', 8080);

const productId = db.insert('products', {
  name: 'Namek Core License',
  price: 99.99,
  category: 'Developer Tools'
});

console.log(`✓ Producto insertado en NamekDB con ID: \x1b[33m${productId}\x1b[0m`);
console.log('Productos en DB:', db.find('products'));

// 2. Developer Tools Demo
console.log('\n\x1b[32m\x1b[1m=== UTILITIES DEMO ===\x1b[0m');
console.log(`Generated UUID: \x1b[36m${Tools.uuid()}\x1b[0m`);
console.log(`MD5 Hash: \x1b[35m${Tools.md5('Hello Namek')}\x1b[0m`);
console.log(`Slugify: \x1b[33m${Tools.slugify('Build CLI Tools Faster')}\x1b[0m`);
