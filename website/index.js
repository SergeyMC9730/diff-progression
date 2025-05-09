const express = require('express')
const app = express()
const port = 80
const fs = require("fs")

const llist = JSON.parse(fs.readFileSync("levels.json"))

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
		console.log("reset: no such rand object")
		res.send("-2")
		return
	}

	seed_table[`${userId}_${seed}`].reset()

	console.log("reset. OBJ: ", seed_table[`${userId}_${seed}`])

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
	random_e.origSeed = obj.origSeed();
	random_e.iterSeed = obj.curSeed();
	res.send(random_e)
});
app.listen(port, () => {
	console.log("ready on port " + port);
});
