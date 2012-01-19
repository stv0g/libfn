<?php
	$count = 10;

	$params = array(
		'color' => (isset($_REQUEST['color'])) ? substr($_REQUEST['color'], 1) : 'ffffff',
		'delay' => (isset($_REQUEST['delay'])) ? $_REQUEST['delay'] : 0,
		'step' => (isset($_REQUEST['step'])) ? $_REQUEST['step'] : 255,
		'mask' => (isset($_REQUEST['mask'])) ? $_REQUEST['mask'] : str_repeat('1', $count),
		'port' => '/dev/ttyUSB0'
	);

	$cmd = '../../src/fnctl fade';
	foreach ($params as $param => $value) {
		$cmd .= ' --' . $param . '=' . $value;
	}

	$return;
	$output = passthru($cmd, $return);

	$json = array(
		'code' => $return,
		'output' => $output,
		'cmd' => $cmd,
		'params' => $params
	);

	header('Content-type: application/json');
	echo json_encode($json);
?>
