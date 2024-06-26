<?php

include(__DIR__ . '\libs\cliprogressbar.class.php');

// Clear console
wcli_clear();

// Create CLIProgressBar instances
$pb1 = new CLIProgressBar(0, 0, ['label' => 'Download 1']);
$pb2 = new CLIProgressBar(0, 1, ['label' => 'Download 2']);

// Change cursor position
wcli_set_cursor_position(0, 4);
wcli_echo("Press ESC to cancel downloads", Yellow);

// Loop for a while
for($i = 0; $i <= 100; $i += 0.1, usleep(100000)) {
    
    // Draw progress bars
    $j = $i * 10;
    if($j <= 100) $pb1->draw($j / 100, round($j) == 100 ? 1 : 0);
    $pb2->draw($i / 100, round($i) == 100 ? 1 : 0);
    
    // Verify if ESCAPE key pressed
    while($key = wcli_get_key_async())
        if($key == VK_ESCAPE)
            break 2;
}
