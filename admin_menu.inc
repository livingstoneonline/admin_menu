<?php
// $Id$

/**
 * Retrieves, sclices and returns the menu tree below /admin.
 */
function admin_menu_build($mid_admin) {
  global $_menu;
  $_admin_menu = array();

  // Temporary access permissions fix for root menu items until #126621 is committed to core.
  foreach ($_menu['items'][$mid_admin]['children'] as $key => $parent) {
    if ($_menu['items'][$parent]['access'] != false || !isset($_menu['items'][$parent]['children'])) {
      continue;
    }
    $is_accessible = false;
    foreach ($_menu['items'][$parent]['children'] as $key => $child) {
      if ($_menu['items'][$child]['access'] == true) {
        $is_accessible = true;
      }
    }
    if ($is_accessible) {
      $_menu['items'][$parent]['access'] = true;
    }
  }
  
  // slice admin menu items into a new menu tree
  $_admin_menu['index']['admin'] = $mid_admin;
  $_admin_menu[$mid_admin] = $_menu['items'][$mid_admin];
  _admin_menu_get_children($_admin_menu, $_admin_menu[$mid_admin]);
  
  // adjust some specific menu items for better user experience
  admin_menu_adjust_items($_admin_menu);
  
  // Allow other modules to integrate with admin_menu.
  foreach (module_implements('admin_menu') as $module) {
    $function = $module .'_admin_menu';
    $function($_admin_menu, TRUE);
  }
  
  return $_admin_menu;
}

function _admin_menu_get_children(&$_admin_menu, &$item) {
  global $_menu;
  
  if (isset($item['children'])) {
    foreach ($item['children'] as $child) {
      // check access permissions
      if (!variable_get('admin_menu_show_all', 0)) {
        $item_is_accessible = !isset($_menu['items'][$child]['access']) || (isset($_menu['items'][$child]['access']) && $_menu['items'][$child]['access']);
      }
      else {
        $item_is_accessible = true;
      }
      // check type
      $item_is_visible = $_menu['items'][$child]['type'] & (MENU_VISIBLE_IN_TREE | MENU_IS_LOCAL_TASK);

      // create the child item if it is accessible and visible
      // Additional condition to hide items linking to parent:
      //   && !($_menu['items'][$child]['type'] & MENU_LINKS_TO_PARENT)
      if ($item_is_accessible && $item_is_visible) {
        $_admin_menu[$child] = $_menu['items'][$child];

        // recurse to child menu items
        if (isset($_menu['items'][$child]['children'])) {
          _admin_menu_get_children($_admin_menu, $_admin_menu[$child]);
        }

        // remove this child item if it is visible in navigation
        unset($_menu['visible'][$child]);
      }
      else {
        // remove child menu item from parent's children list
        $parent_id = $_menu['items'][$child]['pid'];
        if (isset($_menu['items'][$parent_id]['children']) && is_array($_menu['items'][$parent_id]['children'])) {
          $child_key = array_search($child, $_menu['items'][$parent_id]['children']);
          if ($child_key !== false) {
            unset($_admin_menu[$parent_id]['children'][$child_key]);
          }
        }
      }
    }
  }
}

function admin_menu_adjust_items(&$_admin_menu) {
  global $_menu, $user;
  
  // Add the icon containing special links.
  $mid_admin = $_admin_menu['index']['admin'];
  $mid_icon = admin_menu_add_item($_admin_menu, $mid_admin, array(
    'title' => theme('admin_menu_icon'),
    'path' => drupal_get_normal_path(variable_get('site_frontpage', 'node')),
    'weight' => -100,
    'class' => 'admin_menu-icon',
  ));
  $mid_icon_admin = admin_menu_add_item($_admin_menu, $mid_icon, array('title' => t('Administer'), 'path' => 'admin', 'weight' => 10));
  // Reset menu item id of 'admin' in path index.
  $_admin_menu['index']['admin'] = $mid_admin;
  
  // Move 'By module' item if accessible, or remove it.
  $mid_bymodule = $_menu['path index']['admin/by-module'];
  if (user_access('administer site configuration')) {
    admin_menu_move_item($_admin_menu, $mid_bymodule, $mid_icon_admin);
  }
  else {
    admin_menu_remove_item($_admin_menu, 'admin/by-module');
  }
  
  // remove 'By task' menu item
  admin_menu_remove_item($_admin_menu, 'admin/by-task');

  admin_menu_add_item($_admin_menu, $mid_icon, array('title' => t('Run cron'), 'path' => 'admin/logs/status/run-cron', 'weight' => 50));
  if ($user->uid == 1) {
    admin_menu_add_item($_admin_menu, $mid_icon, array('title' => t('Run updates'), 'path' => 'update.php', 'weight' => 50));
  }
  
  // Display drupal.org links.
  if (user_access('display drupal links')) {
    $mid_drupal = admin_menu_add_item($_admin_menu, $mid_icon, array('title' => t('Drupal.org'), 'path' => 'http://drupal.org', 'weight' => 100));
    admin_menu_add_item($_admin_menu, $mid_drupal, array('title' => t('Drupal issue queue'), 'path' => 'http://drupal.org/project/issues/drupal'));
    
    // Add links to project issue queues.
    foreach (module_list(FALSE, FALSE, TRUE) as $module) {
      $info = _module_parse_info_file(drupal_get_path('module', $module) .'/'. $module .'.info');
      if (isset($info['project']) && $info['project'] == 'drupal') {
        continue;
      }
      $url = 'http://drupal.org/project/issues/'. $module;
      // See http://drupal.org/node/97569
      // $url .= !empty($info['version']) ? '/'. $info['version'] : '';
      admin_menu_add_item($_admin_menu, $mid_drupal, array('title' => t('@title issue queue', array('@title' => $info['name'])), 'path' => $url));
    }
  }
  
  // add logout item
  if ($user->uid > 0) {
    $mid_logout = $_menu['path index']['logout'];
    admin_menu_add_item($_admin_menu, $mid_admin, array(
      'title' => t('Logout @name', array('@name' => $user->name)),
      'path' => $_menu['items'][$mid_logout]['path'],
      'weight' => -100,
      'class' => 'admin_menu-action admin_menu-logout',
    ));
  }

  // add create content_type items to Content management > Content
  $mid_node_add = $_menu['path index']['node/add'];
  $mid_content = $_menu['path index']['admin/content/node'];
  admin_menu_copy_items($_admin_menu, $mid_node_add, $mid_content, t('Add !title'));
}

/**
 * Recursively copy menu items from a source parent menu item to a target menu
 * item.
 * 
 * @param _admin_menu
 *   The sliced administration menu array.
 * @param source_pid
 *   A source parent menu item id from which children shall be copied.
 * @param target_pid
 *   A target parent menu item id.
 * @param title
 *   An optional string containing the token !title, that has already been
 *   passed through t().
 * @param tree
 *   Whether to rebuild hierarchy from source parent menu item. Defaults to
 *   true.
 */
function admin_menu_copy_items(&$_admin_menu, $source_pid, $target_pid, $title = NULL, $tree = TRUE) {
  global $_menu;
  
  if (isset($_admin_menu[$target_pid]) && isset($_menu['items'][$source_pid]['children'])) {
    foreach ($_menu['items'][$source_pid]['children'] as $mid) {
      $_admin_menu[$mid] = $_menu['items'][$mid];
      if (isset($title)) {
        $_admin_menu[$mid]['title'] = check_plain(strtr($title, array('!title' => $_admin_menu[$mid]['title'])));
      }
      $_admin_menu[$mid]['pid'] = $target_pid;
      // Only add child to target menu item if it does not already exist.
      if (!in_array($mid, $_admin_menu[$target_pid]['children'])) {
        $_admin_menu[$target_pid]['children'][] = $mid;
      }
      
      // Recurse into children.
      if (isset($_menu['items'][$mid]['children']) && count($_menu['items'][$mid]['children'])) {
        if ($tree) {
          admin_menu_copy_items($_admin_menu, $mid, $mid, $title);
        }
        else {
          admin_menu_copy_items($_admin_menu, $mid, $target_pid, $title, FALSE);
          unset($_admin_menu[$mid]['children']);
          // Note: Uncomment following lines to remove unnecessary parent items.
          // unset($_admin_menu[$target_pid]['children'][array_search($mid, $_admin_menu[$target_pid]['children'])]);
          // unset($_admin_menu[$mid]);
        }
      }
    }
  }
}

/**
 * Moves the child pointer for a menu item to a new parent.
 * 
 * @param _admin_menu
 *      The sliced administration menu.
 * @param mid
 *      The menu item id of the item to move.
 * @param pid
 *      The new parent id.
 */
function admin_menu_move_item(&$_admin_menu, $mid, $pid) {
  global $_menu;
  
  if (isset($_admin_menu[$mid]) && isset($_admin_menu[$pid])) {
    // remove current child pointer
    admin_menu_remove_item($_admin_menu, $_admin_menu[$mid]['path']);
    
    // insert new child pointer
    $_admin_menu[$mid]['pid'] = $pid;
    $_admin_menu[$pid]['children'][] = $mid;
    return true;
  }
  else {
    return false;
  }
}

/**
 * Removes the child pointer for a menu item.
 * 
 * @param _admin_menu
 *      The sliced administration menu.
 * @param path
 *      The menu path to remove.
 */
function admin_menu_remove_item(&$_admin_menu, $path) {
  global $_menu;
  
  $mid = $_menu['path index'][$path];
  if (isset($_admin_menu[$mid])) {
    $pid = $_admin_menu[$mid]['pid'];
    $child_key = array_search($mid, $_admin_menu[$pid]['children']);
    if ($child_key !== false) {
      unset($_admin_menu[$pid]['children'][$child_key]);
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

/**
 * Render an icon to display in the Administration Menu.
 *
 * @ingroup themeable
 */
function theme_admin_menu_icon() {
  return '<img class="admin_menu-icon" src="'. check_url(base_path() . 'misc/favicon.ico') .'" width="16" height="16" alt="" />';
}

