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
  return array('s3' => NULL);
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
  return (d()->type == 'site' && d()->profile != 'hostmaster');
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

// Allow s3 services to respond to normal provision events.
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
    drush_log('S3 access key received from the frontend (d()->access_key_id): ' . d()->access_key_id);
    drush_log('S3 secret access key received from the frontend (d()->secret_access_key): ' . d()->secret_access_key);
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
