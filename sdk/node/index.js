const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const http = require('http');
const https = require('https');
const readline = require('readline');

// ==========================================
// 1. NAMEK NOSQL DB FOR NODE.JS
// ==========================================
class NamekDB {
  constructor(filepath = 'namek_db.json') {
    this.filepath = path.resolve(filepath);
    this.kvStore = {};
    this.collections = {};
    this.load();
  }

  load() {
    if (fs.existsSync(this.filepath)) {
      try {
        const raw = fs.readFileSync(this.filepath, 'utf-8');
        const parsed = JSON.parse(raw);
        this.kvStore = parsed.kv || {};
        this.collections = parsed.collections || {};
        return true;
      } catch (err) {
        return false;
      }
    }
    return false;
  }

  save() {
    try {
      const data = {
        kv: this.kvStore,
        collections: this.collections
      };
      fs.writeFileSync(this.filepath, JSON.stringify(data, null, 2), 'utf-8');
      return true;
    } catch (err) {
      return false;
    }
  }

  // Key-Value API
  set(key, value) {
    this.kvStore[key] = String(value);
    this.save();
  }

  get(key, defaultValue = null) {
    return this.kvStore.hasOwnProperty(key) ? this.kvStore[key] : defaultValue;
  }

  delete(key) {
    if (this.kvStore.hasOwnProperty(key)) {
      delete this.kvStore[key];
      this.save();
      return true;
    }
    return false;
  }

  keys() {
    return Object.keys(this.kvStore);
  }

  // Document Store API
  insert(collectionName, document) {
    if (!this.collections[collectionName]) {
      this.collections[collectionName] = {};
    }
    const docId = crypto.randomUUID();
    const docData = { _id: docId, ...document };
    this.collections[collectionName][docId] = docData;
    this.save();
    return docId;
  }

  find(collectionName, query = null) {
    const col = this.collections[collectionName] || {};
    const docs = Object.values(col);
    if (!query) return docs;
    return docs.filter(doc => {
      return Object.entries(query).every(([k, v]) => doc[k] === v);
    });
  }

  findOne(collectionName, docId) {
    return (this.collections[collectionName] || {})[docId] || null;
  }

  remove(collectionName, docId) {
    if (this.collections[collectionName] && this.collections[collectionName][docId]) {
      delete this.collections[collectionName][docId];
      this.save();
      return true;
    }
    return false;
  }
}

// ==========================================
// 2. NAMEK CLI BUILDER FOR NODE.JS
// ==========================================
class CLIBuilder {
  constructor(appName, version = '1.0.0', description = '') {
    this.appName = appName;
    this.version = version;
    this.description = description;
    this.commands = {};
  }

  addCommand(name, description, handler) {
    this.commands[name] = { description, handler };
  }

  async prompt(question, defaultValue = '') {
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    const suffix = defaultValue ? ` (${defaultValue})` : '';
    return new Promise(resolve => {
      rl.question(`\x1b[36m?\x1b[0m \x1b[1m${question}\x1b[0m${suffix}: `, answer => {
        rl.close();
        resolve(answer.trim() || defaultValue);
      });
    });
  }

  async confirm(question, defaultYes = true) {
    const hint = defaultYes ? '[Y/n]' : '[y/N]';
    const res = await this.prompt(`${question} ${hint}`);
    if (!res) return defaultYes;
    return ['y', 'yes', 's', 'si'].includes(res.toLowerCase());
  }

  run(args = process.argv.slice(2)) {
    if (args.length === 0 || args.includes('--help') || args.includes('-h')) {
      console.log(`\x1b[36m\x1b[1m${this.appName} v${this.version}\x1b[0m`);
      if (this.description) console.log(this.description);
      console.log('\n\x1b[33m\x1b[1mCOMANDOS DISPONIBLES:\x1b[0m');
      Object.entries(this.commands).forEach(([cmd, info]) => {
        console.log(`  \x1b[32m${cmd.padEnd(15)}\x1b[0m ${info.description}`);
      });
      return;
    }

    const cmdName = args[0];
    if (this.commands[cmdName]) {
      this.commands[cmdName].handler(args.slice(1));
    } else {
      console.error(`\x1b[31mError: Comando desconocido '${cmdName}'\x1b[0m`);
    }
  }
}

// ==========================================
// 3. NAMEK DEVELOPER TOOLS FOR NODE.JS
// ==========================================
const Tools = {
  uuid() {
    return crypto.randomUUID();
  },
  sha256(text) {
    return crypto.createHash('sha256').update(text).digest('hex');
  },
  md5(text) {
    return crypto.createHash('md5').update(text).digest('hex');
  },
  slugify(text) {
    return text.toLowerCase().trim().replace(/[^\w\s-]/g, '').replace(/[\s_-]+/g, '-');
  }
};

module.exports = {
  NamekDB,
  CLIBuilder,
  Tools
};
