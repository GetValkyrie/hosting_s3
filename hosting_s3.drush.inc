<?php

/**
 * @file
 * Drush integration for the S3 service.
 */

/**
 * Implements hook_hosting_site_context_options().
 */
function hosting_s3_hosting_site_context_options(&$task) {
  $fields = array('s3_access_key_id', 's3_secret_access_key');
  foreach ($fields as $field) {
    if (isset($task->ref->$field) && !empty($task->ref->$field)) {
      $task->context_options[$field] = $task->ref->$field;
    }
    else {
      $task->context_options[$field] = 'null';
    }
  }
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_s3_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    $fields = array('s3_access_key_id', 's3_secret_access_key', 's3_bucket_name');
    foreach ($fields as $field) {
      if (isset($context->$field) && !empty($context->$field)) {
        $node->$field = $context->$field;
      }
    }
  }
}

/**
 * Implements hook_post_hosting_TASK_TYPE_task().
 */
function hosting_s3_post_hosting_install_task($task, $data) {
  if ($task->ref->type == 'site' && isset($data['context']['s3_bucket_name'])) {
    // Save the generated bucket name in the site node.
    // See: Provision_Service_s3::validate_bucket_name().
    $task->ref->s3_bucket_name = $data['context']['s3_bucket_name'];
  }
}

/**
 * Implements drush_HOOK_post_hosting_task().
 *
 * We do not use the hook_post_hosting_TYPE_task() hook because it runs too
 * early, we need to run after the backup is run and returns the backup bid to
 * the frontend. So this (clunky) implementation is similar to
 * drush_hosting_post_hosting_task() in hosting/task.hosting.inc.
 *
 * See: https://github.com/drush-ops/drush/pull/462
 */
function drush_hosting_s3_post_hosting_task($tid) {
  $task = &drush_get_context('HOSTING_TASK');
  $data = drush_get_context('HOSTING_DRUSH_OUTPUT');
  if ($task->task_type == 'backup' && isset($data['context']['s3_backup_name'])) {
    // Save the backup bucket name associated with the backup ID.
    // See: Provision_Service_s3::pre_backup().
    $bid = hosting_s3_get_latest_site_backup($task->ref->nid);
    $bucket = $data['context']['s3_backup_name'];
    hosting_s3_save_backup($bid, $bucket);
  }
}

/**
 * Implements drush_HOOK_pre_hosting_task()
 */
function drush_hosting_s3_pre_hosting_task() {
  $task =& drush_get_context('HOSTING_TASK');
  if ($task->task_type == 'backup-delete') {
    $bids = array_keys($task->task_args);
    $buckets = array();
    foreach ($bids as $bid) {
      $buckets[] = hosting_s3_load_backup($bid);
    }
    // Pass this to the backend.
    // See: Provision_Service_s3::post_backup_delete().
    $task->options['s3_backups_to_delete'] = $buckets;
  }
}

/**
 * Implements drush_HOOK_post_hosting_TASK_TYPE_task().
 */
function drush_hosting_s3_post_hosting_backup_delete_task($task, $data) {
  $bids = array_keys($task->task_args);
  foreach ($bids as $bid) {
    hosting_s3_delete_backup($bid);
  }
}
