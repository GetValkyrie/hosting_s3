<?php

/**
 * @file
 * Drush integration for the S3 service.
 */

/**
 * Implements hook_hosting_site_context_options().
 */
function hosting_s3_hosting_site_context_options(&$task) {
  if (isset($task->ref->access_key_id) && !empty($task->ref->access_key_id)) {
    $task->context_options['access_key_id'] = $task->ref->access_key_id;
  }
  else {
    $task->context_options['access_key_id'] = 'null';
  }
  if (isset($task->ref->secret_access_key) && !empty($task->ref->secret_access_key)) {
    $task->context_options['secret_access_key'] = $task->ref->secret_access_key;
  }
  else {
    $task->context_options['secret_access_key'] = 'null';
  }
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_s3_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    if (isset($context->access_key_id) && !empty($context->access_key_id)) {
      $node->access_key_id = $context->access_key_id;
    }
    if (isset($context->secret_access_key) && !empty($context->secret_access_key)) {
      $node->secret_access_key = $context->secret_access_key;
    }
  }
}
