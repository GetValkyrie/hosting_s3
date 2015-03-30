<?php

/**
 * Register our directory as a place to find provision classes.
 */
function provision_s3_provision_register_autoload() {
  static $loaded = FALSE;
  if (!$loaded) {
    $loaded = TRUE;
    provision_autoload_register_prefix('Provision_', dirname(__FILE__));
  }
}

/**
 * Implements hook_drush_init().
 */
function provision_s3_drush_init() {
  provision_s3_provision_register_autoload();
}

/**
 *  Implements hook_provision_services().
 */
function provision_s3_provision_services() {
  provision_s3_provision_register_autoload();
  return array('s3' => 's3');
}

/**
 * if we should hook this module at all
 *
 * this is necessary because the drush extension may be deployed in the
 * hostmaster site-specific modules directory, in which case it will load even
 * if the feature is disabled in the frontend.
 *
 * for now we just skip hostmaster sites, but we should really be checking
 * for drush_get_option('hosting_features') or something to that effect.
 *
 * XXX: this should really be handled by Aegir core, not contrib modules.
 */
function _provision_s3_enabled() {
  return (
    d()->type == 'site' &&           // S3 only applies to sites
    d()->profile != 'hostmaster' &&  // Let's be extra careful w/ Aegir.
    isset(d()->s3_access_key_id) &&     // Both credentials need to be set
    !is_null(d()->s3_access_key_id) &&
    isset(d()->s3_secret_access_key) &&
    !is_null(d()->s3_secret_access_key)
  );
}

/**
 * Implements hook_provision_drupal_create_directories_alter().
 */
function provision_s3_provision_drupal_create_directories_alter(&$dirs, $url) {
  if (_provision_s3_enabled()) {
    return d()->service('s3')->create_directories_alter($dirs, $url);
  }
}

/**
 * Implements hook_provision_provision_drupal_chgrp_directories_alter().
 */
function provision_s3_provision_drupal_chgrp_directories_alter(&$chgrp, $url) {
  if (_provision_s3_enabled()) {
    return d()->service('s3')->chgrp_directories_alter($chgrp, $url);
  }
}

/**
 * Implements hook_provision_drupal_config().
 */
function provision_s3_provision_drupal_config($uri, $data) {
  if (_provision_s3_enabled()) {
    return d()->service('s3')->drupal_config($uri, $data);
  }
}

// Allow s3 services to respond to normal provision events.

function drush_provision_s3_provision_install_validate() {
  if (_provision_s3_enabled()) {
    return d()->service('s3')->install_validate();
  }
}
function drush_provision_s3_pre_provision_install() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_install();
  }
}
function drush_provision_s3_post_provision_install() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_install();
  }
}
function drush_provision_s3_pre_provision_delete() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_delete();
  }
}
function drush_provision_s3_post_provision_delete() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_delete();
  }
}
function drush_provision_s3_pre_provision_verify() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_verify();
  }
}
function drush_provision_s3_post_provision_verify() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_verify();
  }
}
function drush_provision_s3_pre_provision_migrate() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_migrate();
  }
}
function drush_provision_s3_post_provision_migrate() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_migrate();
  }
}
function drush_provision_s3_pre_provision_clone() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_clone();
  }
}
function drush_provision_s3_post_provision_clone() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_clone();
  }
}
function drush_provision_s3_pre_provision_backup() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_backup();
  }
}
function drush_provision_s3_pre_provision_backup_rollback() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_backup_rollback();
  }
}
function drush_provision_s3_post_provision_backup() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_backup();
  }
}
function drush_provision_s3_pre_provision_restore() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_restore();
  }
}
function drush_provision_s3_post_provision_restore() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_restore();
  }
}
function drush_provision_s3_pre_provision_backup_delete() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->pre_backup_delete();
  }
}
function drush_provision_s3_post_provision_backup_delete() {
  if (_provision_s3_enabled()) {
    d()->service('s3')->post_backup_delete();
  }
}
