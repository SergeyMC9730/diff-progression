const express = require('express')
const app = express()
const port = 80
const fs = require("fs")

const llist = JSON.parse(fs.readFileSync("levels.json"))

String.prototype.hashCode = function() {
	var hash = 0, i, chr;
	if (this.length === 0) return hash;
	for (i = 0; i < this.length; i++) {
		chr = this.charCodeAt(i);
		hash = ((hash << 5) - hash) + chr;
		hash |= 0; // Convert to 32bit integer
	}
	return hash;
}


class LevelRand {
	constructor(seed = 0) {
		this.__stdlib_seed = seed;
		this.__original_seed = seed;
		this.levels = []
	}

	rand() {
		this.__stdlib_seed = this.__stdlib_seed * 12 + 12345;
		console.log(this.__stdlib_seed > 0xFFFFFFFF)
		if (this.__stdlib_seed > 0xFFFFFFFF) {
			this.__stdlib_seed %= 0xFFFFFFFF
		}
		// console.log("NEW SEED %d", this.__stdlib_seed)
		return Math.abs(Math.floor(this.__stdlib_seed / 65536)) % 32768;
	}
	srand(seed = 0) {
		this.__stdlib_seed = seed;
	}

	getRandomArbitrary(min, max) {
		const a = this.rand()
		const b = a % (max - min)
		const c = b + min
		// console.log(a,b,c)
		return c
	}

	curSeed() {
		return this.__stdlib_seed;
	}
	origSeed() {
		return this.__original_seed;
	}

	setCurSeed(seed = 0) {
		this.__stdlib_seed = seed;
	}
	reset() {
		this.__stdlib_seed = this.__original_seed;
		this.levels = []
	}

	playedLevel(lid = 0) {
		return this.levels.includes(lid)
	}
	playLevel(lid = 0) {
		this.levels.push(lid)
	}
}

const rate_std_lookup = {
	"1": "easy",
	"2": "normal",
	"3": "hard",
	"4": "harder",
	"5": "insane"
}
const rate_demon_lookup = {
	"1": "easy",
	"2": "medium",
	"3": "hard",
	"4": "insane",
	"5": "extreme"
}

let seed_table = {}

app.get("/lp/reset", (req, res) => {
	let userId = 0
	let seed = 0

	if (req.query.seed == undefined || req.query.user_id == undefined) {
		console.log("reset: not enough arguments");
		res.send("-1")
		return
	}

	userId = parseInt(req.query.user_id)
	seed = parseInt(req.query.seed)

	if (userId <= 0) {
		console.log("reset: userId is too low")
		res.send("-3")
		return
	}

	if (seed_table[`${userId}_${seed}`] == undefined) {
		// console.log("reset: no such rand object")
		res.send("-2")
		return
	}

	delete seed_table[`${userId}_${seed}`]

	res.send("0")
})

app.get("/lp/get/:diff", (req, res) => {
	let wanted_seed = 0;
	let userId = 0;
	if (req.query.seed == undefined) {
		wanted_seed = Math.floor(Math.random() * 0xFFFF);
	} else {
		wanted_seed = parseInt(req.query.seed);
	}
	if (req.query.user_id == undefined) {} else {
		userId = parseInt(req.query.user_id)
	}
	if (isNaN(userId) || isNaN(wanted_seed)) {
		console.log("reset: one of arguments is NaN")
		if (isNaN(userId)) userId = 1;
		if (isNaN(wanted_seed)) wanted_seed = req.query.seed.hashCode() + 1
	}

	let obj;
	if (seed_table[`${userId}_${wanted_seed}`] == undefined) {
		console.log("creating new rand object for %d", wanted_seed);
		seed_table[`${userId}_${wanted_seed}`] = new LevelRand(wanted_seed);
	}
	obj = seed_table[`${userId}_${wanted_seed}`];
	console.log(obj);

	const _diff = req.params.diff.split("_");
	const rate_level = _diff[0]
	let diff = _diff[1]
	if (rate_level == undefined || diff == undefined) {
		console.log("not enough arguments")
		res.send({})
		return
	}
	let prefix = ""
	let lookup = rate_std_lookup;
	if (rate_level == "std") {
		prefix = ""
	} else if (rate_level == "demon") {
		prefix = "d_"
		lookup = rate_demon_lookup;
	} else {
		console.log("invalid rate level")
		res.send({})
		return
	}

	let isnum = /^\d+$/.test(diff);

	if (isnum) {
		diff = lookup[diff]
	}
	const entry = llist[`levels_${prefix}${diff}`]
	if (entry == undefined) {
		console.log("undefined entry (wanted %s)", `levels_${prefix}${diff}`)
		res.send({})
		return
	}
	const l = entry.length
	const idx = obj.getRandomArbitrary(0, l)
	console.log("idx=%d", idx);
	let random_e = entry[idx]
	if (obj.playedLevel(random_e.levelID)) random_e = entry[0]
	obj.playLevel(random_e.levelID)
	// let ip = req.headers['x-forwarded-for'] || req.connection.remoteAddress
	// console.log("sent this: ", random_e)
	random_e.origSeed = obj.origSeed();
	random_e.iterSeed = obj.curSeed();
	res.send(random_e)
});
app.get("/lp/v2/get/:diff", (req, res) => {
	let wanted_seed = 0;
	let userId = 0;
	if (req.query.seed == undefined) {
		wanted_seed = Math.floor(Math.random() * 0xFFFF);
	} else {

		wanted_seed = parseInt(req.query.seed);
	}
	if (req.query.user_id == undefined) {} else {
		userId = parseInt(req.query.user_id)
	}
	if (isNaN(userId) || isNaN(wanted_seed)) {
		console.log("reset: one of arguments is NaN")
		if (isNaN(userId)) userId = 1;
		if (isNaN(wanted_seed)) wanted_seed = req.query.seed.hashCode() + 1
	}

	let obj;
	if (seed_table[`${userId}_${wanted_seed}`] == undefined) {
		console.log("creating new rand object for %d", wanted_seed);
		seed_table[`${userId}_${wanted_seed}`] = new LevelRand(wanted_seed);
	}
	obj = seed_table[`${userId}_${wanted_seed}`];
	console.log(obj);

	const _diff = req.params.diff.split("_");
	const rate_level = _diff[0]
	let diff = _diff[1]
	if (rate_level == undefined || diff == undefined) {
		console.log("not enough arguments")
		res.send({})
		return
	}
	let prefix = ""
	let lookup = rate_std_lookup;
	if (rate_level == "std") {
		prefix = ""
	} else if (rate_level == "demon") {
		prefix = "d_"
		lookup = rate_demon_lookup;
	} else {
		console.log("invalid rate level")
		res.send({})
		return
	}

	let isnum = /^\d+$/.test(diff);

	if (isnum) {
		diff = lookup[diff]
	}
	const entry = llist[`levels_${prefix}${diff}`]
	if (entry == undefined) {
		console.log("undefined entry (wanted %s)", `levels_${prefix}${diff}`)
		res.send({})
		return
	}
	const l = entry.length
	let level_is_available = false
	let random_e = {}
	let random_tries = 0
	while (!level_is_available && random_tries <= l) {
		const idx = obj.getRandomArbitrary(0, l)
		let random_entry = entry[idx]
		if (obj.playedLevel(random_e.levelID)) {
			console.log("already played level %d; attempt %d", random_e.levelID, random_tries)
			random_tries++
			continue
		} else {
			level_is_available = true
			random_e = random_entry
			break
		}
		random_tries++
	}
	if (!level_is_available) {
		console.log("could not find level. failbacking to index 0")
		random_e = entry[0]
	}
	obj.playLevel(random_e.levelID)
	// let ip = req.headers['x-forwarded-for'] || req.connection.remoteAddress
	// console.log("sent this: ", random_e)
	random_e.origSeed = obj.origSeed();
	random_e.iterSeed = obj.curSeed();
	res.send(random_e)
});
app.get("/lp/v2/news", (req, res) => {
	str = fs.readFileSync("./news.txt").toString("utf-8")
	res.send(str)
})
app.get("/lp/v2/stats", (req, res) => {
	const stats = {
		"sessions": Object.keys(seed_table).length
	}
	res.send(stats)
})
app.listen(port, () => {
	console.log("ready on port " + port);
});
