<?php

  // cleanup requested filepath
  $file = isset($_GET['f']) ? $_GET['f'] : 'docdown';
  $file = preg_replace('#(\.*[\/])+#', '', $file);
  $file .= preg_match('/\.[a-z]+$/', $file) ? '' : '.php';

  // output filename
  if (isset($_GET['o'])) {
    $output = $_GET['o'];
  } else if (isset($_SERVER['argv'][1])) {
    $output = $_SERVER['argv'][1];
  } else {
    $output = basename($file);
  }

  /*--------------------------------------------------------------------------*/

  require('../docdown.php');

  // generate Markdown
  $markdown = docdown(array(
    'path' => '../' . $file,
    'title' => 'Docdown <sup>v1.0.0</sup>',
    'url'  => 'https://github.com/jdalton/docdown/blob/master/docdown.php'
  ));

  // save to a .md file
  file_put_contents($output . '.md', $markdown);

  // print
  header('Content-Type: text/plain;charset=utf-8');
  echo $markdown . PHP_EOL;

?>