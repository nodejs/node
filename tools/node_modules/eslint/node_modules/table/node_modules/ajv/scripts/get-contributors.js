// Credit for the script goes to svelte:
// https://github.com/sveltejs/svelte/blob/ce3a5791258ec6ecf8c1ea022cb871afe805a45c/site/scripts/get-contributors.js

const fs = require("fs")
const fetch = require("node-fetch")
const Jimp = require("jimp")

process.chdir(__dirname)

const base = `https://api.github.com/repos/ajv-validator/ajv/contributors`
const {GH_TOKEN_PUBLIC} = process.env

const SIZE = 64

async function main() {
  const contributors = []
  let page = 1

  // eslint-disable-next-line no-constant-condition
  while (true) {
    const res = await fetch(`${base}?per_page=100&page=${page++}`, {
      headers: {Authorization: `token ${GH_TOKEN_PUBLIC}`},
    })
    const list = await res.json()
    if (list.length === 0) break
    contributors.push(...list)
  }

  const bots = ["dependabot-preview[bot]", "greenkeeper[bot]", "greenkeeperio-bot"]

  const authors = contributors
    .filter((a) => !bots.includes(a.login))
    .sort((a, b) => b.contributions - a.contributions)

  const sprite = new Jimp(SIZE * authors.length, SIZE)

  for (let i = 0; i < authors.length; i += 1) {
    const author = authors[i]
    console.log(`${i + 1} / ${authors.length}: ${author.login}`)
    const image_data = await fetch(author.avatar_url)
    const buffer = await image_data.arrayBuffer()
    const image = await Jimp.read(buffer)
    image.resize(SIZE, SIZE)
    sprite.composite(image, i * SIZE, 0)
  }

  await sprite.quality(80).write(`../docs/.vuepress/components/Contributors/contributors.jpg`)

  const str = `[\n  ${authors.map((a) => `"${a.login}"`).join(",\n  ")},\n]\n`

  fs.writeFileSync(
    `../docs/.vuepress/components/Contributors/_contributors.js`,
    `module.exports = ${str}`
  )
}

main()
