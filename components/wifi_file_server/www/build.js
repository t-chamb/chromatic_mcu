#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { minify: minifyHTML } = require('html-minifier');
const CleanCSS = require('clean-css');
const { minify: minifyJS } = require('terser');

async function build() {
  console.log('Building web UI...');
  
  // Read source files
  const html = fs.readFileSync('index.html', 'utf8');
  const css = fs.readFileSync('style.css', 'utf8');
  const js = fs.readFileSync('app.js', 'utf8');
  
  // Minify HTML
  const minifiedHTML = minifyHTML(html, {
    collapseWhitespace: true,
    removeComments: true,
    minifyCSS: true,
    minifyJS: true
  });
  
  // Minify CSS
  const minifiedCSS = new CleanCSS({}).minify(css).styles;
  
  // Minify JS
  const minifiedJS = (await minifyJS(js)).code;
  
  // Write minified files
  fs.writeFileSync('index.html.min', minifiedHTML);
  fs.writeFileSync('style.css.min', minifiedCSS);
  fs.writeFileSync('app.js.min', minifiedJS);
  
  console.log('Build complete!');
  console.log(`HTML: ${html.length} -> ${minifiedHTML.length} bytes`);
  console.log(`CSS: ${css.length} -> ${minifiedCSS.length} bytes`);
  console.log(`JS: ${js.length} -> ${minifiedJS.length} bytes`);
}

build().catch(console.error);
